#include "lvgl_dashboard.h"

namespace esphome {
namespace lvgl_dashboard {

#define TAG "LVGLD"

MdiFontCapable* icons_ = new MdiFontCapable();

template <typename T>
void lvgl_event_listener_(lv_event_t* event) {
    // ESP_LOGD(TAG, "lvgl_event_listener_: %d, %p", event->code, lv_event_get_user_data(event));
    T* listener = (T*)(lv_event_get_user_data(event));
    listener->on_event(event);
}

template <typename T>
void lvgl_tap_event_listener_(lv_event_t* event) {
    // ESP_LOGD(TAG, "lvgl_tap_event_listener_: %d, %p", event->code, lv_event_get_user_data(event));
    T* listener = (T*)(lv_event_get_user_data(event));
    listener->on_tap_event(lv_event_get_code(event), event);
}

template <typename T>
void subscribe_to_tap_events_(lv_obj_t* obj, T* receiver) {
    lv_obj_add_event_cb(obj, lvgl_tap_event_listener_<T>, LV_EVENT_LONG_PRESSED, receiver); // 5
    lv_obj_add_event_cb(obj, lvgl_tap_event_listener_<T>, LV_EVENT_SHORT_CLICKED, receiver); // 4
}

uint8_t* mem_alloc_(size_t size) {
    #ifdef USE_HOST
    return (uint8_t*)lv_mem_alloc(size);
    #else
    return (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    #endif
}

uint8_t* mem_realloc_(void* ptr, size_t size) {
    #ifdef USE_HOST
    return (uint8_t*)lv_mem_realloc(ptr, size);
    #else
    return (uint8_t*)heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    #endif
}

void mem_free_(void* ptr) {
    #ifdef USE_HOST
    lv_mem_free(ptr);
    #else
    heap_caps_free(ptr);
    #endif
}

struct SpiRamAllocator {
    void* allocate(size_t size) {
        return mem_alloc_(size);
    }

    void deallocate(void* pointer) {
        mem_free_(pointer);
    }

    void* reallocate(void* ptr, size_t new_size) {
        return mem_realloc_(ptr, new_size);
    }

};

using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

bool json_parse_(std::string json_doc, std::function<void(JsonObject)> &&fn) {
    size_t doc_size = 2.5 * json_doc.size();
    DeserializationError err;
    do {
        auto doc_ = SpiRamJsonDocument(doc_size);
        if (doc_.capacity() == 0) {
            ESP_LOGW(TAG, "json_parse_:: Failed to allocate memory for Json");
            return false;
        }
        err = deserializeJson(doc_, json_doc);
        doc_.shrinkToFit();
        JsonObject root = doc_.as<JsonObject>();
        if (err == DeserializationError::Ok) {
            ESP_LOGV(TAG, "json_parse_: Deserialized: %u / %u", json_doc.size(), doc_size);
            fn(root);
            return true;
        } else {
            ESP_LOGD(TAG, "json_parse_:: Failed to deserialize Json: %u [%s]", json_doc.size(), err.c_str());
        }
        doc_size *= 2;
    } while (err == DeserializationError::NoMemory);
    return false;
}

static const uint8_t *_mdi_get_glyph_bitmap(const lv_font_t *font, uint32_t unicode_letter) {
    auto *mdi_font_ = (MdiFont*) font->dsc;
    const auto *gd = mdi_font_->get_glyph_data(unicode_letter);
    if (gd == 0) return nullptr;
    return gd;
}

static bool _mdi_get_glyph_dsc_cb(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t unicode_letter, uint32_t next) {
    auto *mdi_font_ = (MdiFont*) font->dsc;
    if (!mdi_font_->create_glyph_dsc(unicode_letter, dsc)) {
        return false;
    }
    return true;
}

bool MdiFont::create_glyph_dsc(uint32_t unicode_letter, lv_font_glyph_dsc_t *dsc) {
    if (auto search = this->glyphs_.find(unicode_letter); search != this->glyphs_.end()) {
        uint8_t* data = search->second;
        dsc->adv_w = data[0] + data[2];
        dsc->ofs_x = data[0];
        dsc->ofs_y = this->size_ - data[3] - data[1];
        dsc->box_w = data[2];
        dsc->box_h = data[3];
        dsc->is_placeholder = 0;
        dsc->bpp = 1;
        return true;
    }
    return false;
}

const uint8_t* MdiFont::get_glyph_data(uint32_t unicode_letter) {
    if (auto search = this->glyphs_.find(unicode_letter); search != this->glyphs_.end()) {
        uint8_t* data = search->second;
        return &data[5];
    }
    return 0;
}

static uint32_t get_rle_size(uint8_t* buffer, uint32_t len) {
    uint32_t result = 0;
    for (int i = 0; i < len - 1; i += 2) {
        result += buffer[i];
    }
    return result;
}

static void decompress_rle(uint8_t* buffer, uint32_t len, uint8_t* to_buffer) {
    for (int i = 0, j = 0; i < len - 1; i += 2) {
        for (int k = 0; k < buffer[i]; k++, j++) {
            to_buffer[j] = buffer[i+1];
        }
    }
}

#define ICON_HEADER 5
#define ICON_FONT_CODE_START 0x20

static uint32_t get_b64_size(uint8_t prefix, std::string b64_data) {
    auto b64_decoded = base64_decode(b64_data);
    if (b64_decoded[prefix] == 1) {
        auto rle_size = get_rle_size(b64_decoded.data() + prefix + 1, b64_decoded.size() - prefix - 1);
        return prefix + rle_size;
    } else {
        return b64_decoded.size() - prefix - 1;
    }
}

static void data_from_b64(uint8_t prefix, std::string b64_data, uint8_t* buffer) {
    auto b64_decoded = base64_decode(b64_data);
    memcpy(buffer, b64_decoded.data(), prefix);
    if (b64_decoded[prefix] == 1) {
        auto rle_size = get_rle_size(b64_decoded.data() + prefix + 1, b64_decoded.size() - prefix - 1);
        decompress_rle(b64_decoded.data() + prefix + 1, b64_decoded.size() - prefix - 1, &buffer[prefix]);
    } else {
        memcpy(&buffer[prefix], b64_decoded.data() + prefix + 1, b64_decoded.size() - prefix - 1);
    }
}

uint32_t MdiFont::add_glyph(std::string icon, std::string b64_data) {
    if (auto search = this->codes_.find(icon); search != this->codes_.end()) 
        return search->second;
    auto b64_decoded = base64_decode(b64_data);

    uint16_t size = b64_decoded.size();
    uint8_t* buf;
    if (b64_decoded[4] == 1) {
        // rle
        auto rle_size = get_rle_size(b64_decoded.data() + ICON_HEADER, b64_decoded.size() - ICON_HEADER);
        ESP_LOGD(TAG, "add_glyph: icon: %s, rle size: %lu", icon.c_str(), rle_size);
        size = ICON_HEADER + rle_size;

        buf = mem_alloc_(size);
        memcpy(buf, b64_decoded.data(), ICON_HEADER);
        decompress_rle(b64_decoded.data() + ICON_HEADER, b64_decoded.size() - ICON_HEADER, &buf[ICON_HEADER]);
    } else {
        ESP_LOGD(TAG, "add_glyph: icon: %s, raw size: %u", icon.c_str(), size);
        buf = mem_alloc_(size);
        memcpy(buf, b64_decoded.data(), size);
    }
    uint32_t code = ICON_FONT_CODE_START + this->codes_.size() + 1;
    this->codes_[icon] = code;
    this->glyphs_[code] = buf;
    ESP_LOGD(TAG, "add_glyph: %u - %u - %u - %u", buf[0], buf[1], buf[2], buf[3]);
    return code;
}

void MdiFont::destroy() {
    for (auto entry : this->glyphs_) {
        mem_free_(entry.second);
    }
    this->glyphs_.clear();
    this->codes_.clear();
}


MdiFont::MdiFont(int size) {
    this->size_ = size;
    this->lv_font_.dsc = this;
    this->lv_font_.line_height = size + 1;
    this->lv_font_.base_line = 0;
    this->lv_font_.get_glyph_dsc = _mdi_get_glyph_dsc_cb;
    this->lv_font_.get_glyph_bitmap = _mdi_get_glyph_bitmap;
    this->lv_font_.subpx = LV_FONT_SUBPX_NONE;
    this->lv_font_.underline_position = -1;
    this->lv_font_.underline_thickness = 1;
}

uint8_t* WithDataBuffer::create_data_(uint32_t size) {
    if (this->data_ != 0) {
        mem_free_(this->data_);
    }
    this->data_ = mem_alloc_(size);
    this->data_size_ = size;
    return this->data_;
}
bool WithDataBuffer::set_data_(int32_t* data, int size, int offset, int total_size) {
    if (offset == 0) {
        this->create_data_(total_size * 4);
    }
    memcpy(&this->data_[offset * 4], data, size * 4);
    if ((offset + size) == total_size) {
        return true;
    }
    return false;
}
void WithDataBuffer::destroy_() {
    if (this->data_ != 0) {
        mem_free_(this->data_);
        this->data_ = 0;
    }
}


void DashboardButton::on_event(lv_event_t* event) {
    auto code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        lv_obj_add_state(this->line_, LV_STATE_PRESSED);
    }
    if (code == LV_EVENT_RELEASED) {
        lv_obj_clear_state(this->line_, LV_STATE_PRESSED);
    }
    ESP_LOGD(TAG, "DashboardButton::on_event: %d", code);
    if (code == LV_EVENT_GESTURE) {
        auto dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        ESP_LOGD(TAG, "DashboardButton::on_event: gesture %d", dir);
    }
}

void DashboardButton::on_tap_event(lv_event_code_t code, lv_event_t* event) {
    if ((this->listener_.listener != 0) && (this->listener_.listener->on_button(this->listener_.index, code))) {
        return;
    }
    if (code == LV_EVENT_SHORT_CLICKED) {
        this->switch_->toggle();
    }
}

void DashboardButton::init(lv_obj_t* obj) {
    lv_style_init(&btn_style_normal_);
    lv_style_set_pad_all(&btn_style_normal_, LVD_PADDING);
    lv_style_set_radius(&btn_style_normal_, LVD_BORDER_RADIUS);

    lv_style_init(&btn_style_pressed_);
    lv_style_set_bg_color(&btn_style_pressed_, LVD_BTN_PRESSED_COLOR);
    lv_style_set_bg_opa(&btn_style_pressed_, LV_OPA_50);
    // lv_style_set_translate_y(&btn_style_pressed_, LVD_PRESS_TRANSLATE_Y);

    lv_style_init(&btn_line_style_normal_);
    lv_style_set_bg_opa(&btn_line_style_normal_, LV_OPA_COVER);
    lv_style_set_bg_color(&btn_line_style_normal_, LVD_SWITCH_LINE_COLOR);
    lv_style_set_radius(&btn_line_style_normal_, 3);
    lv_style_set_width(&btn_line_style_normal_, lv_pct(100));
    lv_style_set_height(&btn_line_style_normal_, 7);

    lv_style_init(&btn_line_style_pressed_);
    lv_style_set_bg_color(&btn_line_style_pressed_, LVD_SWITCH_PRESSED_LINE_COLOR);

    lv_style_init(&btn_line_style_checked_);
    lv_style_set_bg_color(&btn_line_style_checked_, LVD_SWITCH_ON_LINE_COLOR);
}

void DashboardButton::update_state(bool state) {
    if (state) {
        lv_obj_add_state(this->line_, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(this->line_, LV_STATE_CHECKED);
    }
    
}

void DashboardButton::setup(lv_obj_t* root, esphome::switch_::Switch* switch_) {
    this->switch_ = switch_;
    this->root_ = lv_btn_create(root);
    
    lv_obj_set_size(this->root_, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_remove_style_all(this->root_);
    lv_obj_add_style(this->root_, &btn_style_normal_, 0);
    lv_obj_add_style(this->root_, &btn_style_pressed_, LV_STATE_PRESSED);
    lv_obj_add_event_cb(this->root_, lvgl_event_listener_<DashboardButton>, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(this->root_, lvgl_event_listener_<DashboardButton>, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(this->root_, lvgl_event_listener_<DashboardButton>, LV_EVENT_GESTURE, this);
    subscribe_to_tap_events_(this->root_, this);

    lv_obj_add_flag(lv_label_create(this->root_), LV_OBJ_FLAG_HIDDEN);

    this->line_ = lv_obj_create(this->root_);
    lv_obj_remove_style_all(this->line_);
    lv_obj_align(this->line_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    // lv_obj_set_size(this->line_, lv_pct(100), 10);
    lv_obj_add_flag(this->line_, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_add_style(this->line_, &btn_line_style_normal_, 0);
    lv_obj_add_style(this->line_, &btn_line_style_checked_, LV_STATE_CHECKED);
    lv_obj_add_style(this->line_, &btn_line_style_pressed_, LV_STATE_PRESSED);
    lv_obj_set_flex_grow(this->line_, 1);

    lv_obj_add_flag(lv_label_create(this->root_), LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_layout(this->root_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(this->root_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(this->root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(this->root_, LVD_PADDING, 0);

    switch_->add_on_state_callback([this](bool state) {
        this->update_state(state);
    });
    this->update_state(switch_->state);
}

void DashboardButton::set_side_icon(lv_obj_t* obj, JsonObject data) {
    if (data.isNull()) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        icons_->set_icon(obj, data["icon"]);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(obj, LVD_TEXT_COLOR, 0);
    }
}

void DashboardButton::set_value(JsonObject data) {
    JsonObject left = data["left"];
    JsonObject right = data["right"];
    this->set_side_icon(lv_obj_get_child(this->root_, 0), left);
    this->set_side_icon(lv_obj_get_child(this->root_, 2), right);
}

void DashboardButton::destroy() {
    lv_obj_del(this->root_);
}

void DashboardItem::init(lv_obj_t* obj) {
    lv_style_init(&item_style_normal_);
    lv_style_init(&item_style_pressed_);

    lv_style_set_bg_opa(&item_style_normal_, LV_OPA_COVER);
    lv_style_set_radius(&item_style_normal_, LVD_BORDER_RADIUS);
    lv_style_set_pad_all(&item_style_normal_, LVD_PADDING);

    // lv_style_set_translate_y(&item_style_pressed_, LVD_PRESS_TRANSLATE_Y);
    lv_style_set_bg_color(&item_style_pressed_, LVD_BTN_PRESSED_COLOR);
}

void DashboardItem::loop() {

}

// static std::set<int> proxy_events_ = {LV_EVENT_CLICKED, LV_EVENT_LONG_PRESSED};
void DashboardItem::on_tap_event(lv_event_code_t code, lv_event_t* event) {
    ESP_LOGD(TAG, "DashboardItem::on_tap_event: %d", code);
    if (this->listener_.listener != 0) {
        this->listener_.listener->on_item_event(this->listener_.page, this->listener_.item, code);
    }
}

void DashboardItem::request_data() {
    if (this->listener_.listener != 0) {
        this->listener_.listener->on_data_request(this->listener_.page, this->listener_.item);
    }
}

void DashboardItem::set_listener(int page, int item, LvglItemEventListener* listener) {
    this->listener_.item = item;
    this->listener_.page = page;
    this->listener_.listener = listener;
}

MdiFont* MdiFontCapable::get_font(int size) {
    if (auto search = this->fonts_.find(size); search != this->fonts_.end()) 
        return search->second;
    MdiFont* font = new MdiFont(size);
    this->fonts_[size] = font;
    return font;
}

void MdiFontCapable::set_icon(lv_obj_t* obj, JsonObject icon_data) {
    int size = icon_data["size"];
    auto* font = this->get_font(size);
    auto code = font->add_glyph(icon_data["name"], icon_data["data"]);
    lv_obj_set_style_text_font(obj, font->get_lv_font(), 0);
    char txt[2] = {(char)code, 0};
    lv_label_set_text(obj, (char *)&txt);
}

void MdiFontCapable::clear() {
    for (auto it : this->fonts_) {
        it.second->destroy();
        free(it.second);
    }
    this->fonts_.clear();
}

void DashboardItem::set_bg_color(lv_obj_t* obj, JsonObject data) {
    std::string mode = data["ctype"];
    std::string color = data["col"];
    if (color == "") {
        lv_obj_set_style_bg_color(obj, LVD_BTN_BG_COLOR, 0);
        return;
    }
    if (mode == "text") {
        lv_obj_set_style_bg_color(obj, LVD_BTN_BG_COLOR, 0);
        return;
    }
    if (color == "on") {
        lv_obj_set_style_bg_color(obj, LVD_BTN_ON_COLOR, 0);
        return;
    }
    if (color.size() == 7) {
        lv_obj_set_style_bg_color(obj, lv_color_hex((uint32_t)std::stol(color.substr(1), nullptr, 16)), 0);
        return;
    }
    lv_obj_set_style_bg_color(obj, LVD_BTN_BG_COLOR, 0);
}

void DashboardItem::set_text_color(lv_obj_t* obj, JsonObject data) {
    std::string mode = data["ctype"];
    std::string color = data["col"];
    if (color == "") {
        lv_obj_set_style_text_color(obj, LVD_TEXT_COLOR, 0);
        return;
    }
    if (mode == "text") {
        if (color == "on") {
            lv_obj_set_style_text_color(obj, LVD_BTN_ON_COLOR, 0);
            return;
        }
        if (color.size() == 7) {
            lv_obj_set_style_text_color(obj, lv_color_hex((uint32_t)std::stol(color.substr(1), nullptr, 16)), 0);
            return;
        }
        lv_obj_set_style_text_color(obj, LVD_TEXT_COLOR, 0);
        return;
    }
    if (color.size() > 0) {
        lv_obj_set_style_text_color(obj, LVD_BG_COLOR, 0); // BG set - dark text
        return;
    }
}


void ImageItem::set_data(int32_t* data, int size, int offset, int total_size) {
    if (this->set_data_(data, size, offset, total_size)) {
        this->show();
    }
}

void ImageItem::show() {
    ESP_LOGD(TAG, "ImageItem::show");
    this->image_.data_size = this->data_size_;
    this->image_.data = (unsigned char*)this->data_;
    // ESP_LOGD(TAG, "DashboardItem::set_value: image: %d, %d, %lu", this->image_.header.w, this->image_.header.h, this->data_size_);
    lv_img_set_src(lv_obj_get_child(this->root_, 0), &this->image_);
    lv_obj_clear_flag(lv_obj_get_child(this->root_, 0), LV_OBJ_FLAG_HIDDEN);
}

void LayoutItem::set_value(JsonObject data) {
    lv_obj_clean(this->root_);
    JsonArray cols_ = data["cols"];
    for (int i = 0; i < cols_.size(); i++) {
        int fr = cols_[i];
        this->col_dsc_[i] = LV_GRID_FR(fr);
    }
    this->col_dsc_[cols_.size()] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_style_grid_column_dsc_array(this->root_, this->col_dsc_, 0);

    JsonArray rows_ = data["rows"];
    for (int i = 0; i < rows_.size(); i++) {
        int fr = rows_[i];
        this->row_dsc_[i] = LV_GRID_FR(fr);
    }
    this->row_dsc_[rows_.size()] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_style_grid_row_dsc_array(this->root_, this->row_dsc_, 0);
    lv_obj_set_layout(this->root_, LV_LAYOUT_GRID);
    JsonArray items = data["items"];
    int col = 0;
    int row = 0;
    for (int i = 0; i< items.size(); i++) {
        if (col >= cols_.size()) {
            row++;
            col = 0;
        }
        JsonObject item = items[i];

        if (item.containsKey("x")) col = item["x"];
        if (item.containsKey("y")) row = item["y"];
        int cols = 1;
        int rows = 1;
        if (item.containsKey("w")) cols = item["w"];
        if (item.containsKey("h")) rows = item["h"];

        if (item.containsKey("_h")) {
            bool hidden = item["_h"];
            if (hidden) {
                col += cols;
                continue;
            }
        }
        lv_obj_t* obj = lv_label_create(this->root_);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_CENTER, col, cols, LV_GRID_ALIGN_CENTER, row, rows);
        if (item.containsKey("icon")) {
            icons_->set_icon(obj, item["icon"]);
        } else {
            lv_obj_set_style_text_font(obj, lv_theme_get_font_normal(this->root_), 0);
            lv_label_set_text(obj, item["label"]);
        }
        this->set_text_color(obj, item);
        col += cols;
    }
    this->set_bg_color(this->root_, data);
}

void ButtonItem::set_value(JsonObject data) {
    auto* icon_ = lv_obj_get_child(this->root_, 0);
    auto* label_ = lv_obj_get_child(this->root_, 1);
    this->set_bg_color(this->root_, data);
    this->set_text_color(icon_, data);
    this->set_text_color(label_, data);
    icons_->set_icon(icon_, data["icon"]);
    lv_label_set_text(label_, data["name"]);
}

void SensorItem::set_value(JsonObject data) {
    icons_->set_icon(lv_obj_get_child(this->root_, 0), data["icon"]);
    lv_label_set_text(lv_obj_get_child(this->root_, 1), data["name"]);
    lv_label_set_text(lv_obj_get_child(this->root_, 2), data["value"]);
    lv_label_set_text(lv_obj_get_child(this->root_, 3), data["unit"]);
}

void ImageItem::set_value(JsonObject data) {
    JsonObject image = data["image"];
    this->set_bg_color(this->root_, data);
    if (!image.isNull()) {
        ESP_LOGD(TAG, "ImageItem::set_value");
        this->image_.header.always_zero = 0;
        this->image_.header.w = image["width"];
        this->image_.header.h = image["height"];
        this->image_.header.cf = LV_IMG_CF_TRUE_COLOR;
        this->request_data();
    } else {
        lv_obj_add_flag(lv_obj_get_child(this->root_, 0), LV_OBJ_FLAG_HIDDEN);
    }
}

void LocalItem::setup(lv_obj_t* root) {
    DashboardItem::setup(root);
    lv_obj_set_style_bg_color(this->root_, LVD_PANEL_BG_COLOR, 0);
    this->col_dsc_[1] = LV_GRID_TEMPLATE_LAST;
    this->row_dsc_[0] = LV_GRID_FR(7);
    this->row_dsc_[1] = LV_GRID_FR(3);
    this->row_dsc_[2] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_style_grid_row_dsc_array(this->root_, this->row_dsc_, 0);
    lv_obj_set_style_grid_column_dsc_array(this->root_, this->col_dsc_, 0);
    lv_obj_set_layout(this->root_, LV_LAYOUT_GRID);

    lv_obj_t* icon = lv_label_create(this->root_);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_style_text_font(icon, large_mdi_font->get_lv_font(), 0);
    lv_label_set_text(icon, this->def_->icon.c_str());

    lv_obj_t* label = lv_label_create(this->root_);
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_label_set_text(label, this->def_->label.c_str());
    lv_obj_set_style_text_font(label, lv_theme_get_font_normal(root), 0);
}

void LayoutItem::setup(lv_obj_t* root) {
    DashboardItem::setup(root);
    lv_obj_set_style_bg_color(this->root_, LVD_BTN_BG_COLOR, 0);
    subscribe_to_tap_events_(this->root_, this);
}

void ButtonItem::setup(lv_obj_t* root) {
    DashboardItem::setup(root);
    lv_obj_set_style_bg_color(this->root_, LVD_BTN_BG_COLOR, 0);
    this->col_dsc_[1] = LV_GRID_TEMPLATE_LAST;
    this->row_dsc_[0] = LV_GRID_FR(8);
    this->row_dsc_[1] = LV_GRID_FR(2);
    this->row_dsc_[2] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_style_grid_row_dsc_array(this->root_, this->row_dsc_, 0);
    lv_obj_set_style_grid_column_dsc_array(this->root_, this->col_dsc_, 0);
    lv_obj_set_layout(this->root_, LV_LAYOUT_GRID);

    lv_obj_t* icon = lv_label_create(this->root_);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_label_set_text(icon, "");

    lv_obj_t* label = lv_label_create(this->root_);
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_style_text_font(label, lv_theme_get_font_normal(root), 0);
    lv_label_set_text(label, "");
    subscribe_to_tap_events_(this->root_, this);    
}

void SensorItem::setup(lv_obj_t* root) {
    DashboardItem::setup(root);
    lv_obj_set_style_bg_color(this->root_, LVD_PANEL_BG_COLOR, 0);
    this->col_dsc_[0] = 25; this->col_dsc_[2] = LV_GRID_CONTENT; this->col_dsc_[3] = LV_GRID_TEMPLATE_LAST; 
    this->row_dsc_[0] = 25; this->row_dsc_[2] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_style_grid_row_dsc_array(this->root_, this->row_dsc_, 0);
    lv_obj_set_style_grid_column_dsc_array(this->root_, this->col_dsc_, 0);
    lv_obj_set_layout(this->root_, LV_LAYOUT_GRID);

    lv_obj_t* icon = lv_label_create(this->root_);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_label_set_text(icon, "");

    lv_obj_t* label = lv_label_create(this->root_);
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, 1, 2, LV_GRID_ALIGN_END, 0, 1);
    lv_obj_set_style_text_font(label, lv_theme_get_font_normal(root), 0);
    lv_label_set_text(label, "");

    lv_obj_t* value = lv_label_create(this->root_);
    lv_obj_set_grid_cell(value, LV_GRID_ALIGN_END, 0, 2, LV_GRID_ALIGN_END, 1, 1);
    lv_obj_set_style_text_font(value, lv_theme_get_font_large(root), 0);
    lv_label_set_text(value, "");

    lv_obj_t* unit = lv_label_create(this->root_);
    lv_obj_set_grid_cell(unit, LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_END, 1, 1);
    lv_obj_set_style_text_font(unit, lv_theme_get_font_small(root), 0);
    lv_label_set_text(unit, "");
}

void ImageItem::setup(lv_obj_t* root) {
    DashboardItem::setup(root);
    lv_obj_set_style_bg_color(this->root_, LVD_BTN_BG_COLOR, 0);
    lv_obj_t* image = lv_img_create(this->root_);
    lv_obj_center(image);
    lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
    subscribe_to_tap_events_(this->root_, this);

    /*
    this->png_ = pngle_new();
    pngle_set_init_callback(this->png_, on_pngle_init_);
    pngle_set_draw_callback(this->png_, on_pngle_draw_);
    pngle_set_done_callback(this->png_, on_pngle_done_);
    pngle_set_user_data(this->png_, this);
    */
}

void ImageItem::destroy() {
    DashboardItem::destroy();
    WithDataBuffer::destroy_();
}

void DashboardItem::setup(lv_obj_t* root) {
    this->root_ = lv_btn_create(root);
    lv_obj_remove_style_all(this->root_);
    lv_obj_set_size(this->root_, lv_pct(100), lv_pct(100));
    lv_obj_add_style(this->root_, &item_style_normal_, 0);
    lv_obj_add_style(this->root_, &item_style_pressed_, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(this->root_, LVD_PANEL_BG_COLOR, 0);
}

void DashboardItem::destroy() {
    if (this->root_ != 0) {
        lv_obj_del(this->root_);
        this->root_ = 0;
    }
}

DashboardItem* DashboardItem::new_instance(ItemDef* def) {
    std::string layout = def->layout;
    if (layout == "local") return new LocalItem();
    if (layout == "button") return new ButtonItem();
    if (layout == "sensor") return new SensorItem();
    if (layout == "picture") return new ImageItem();
    if (layout == "layout") return new LayoutItem();
    return 0;
}

void DashboardPage::init(lv_obj_t* obj) {
    lv_style_init(&page_style_);
    lv_style_set_pad_column(&page_style_, LVD_PADDING);
    lv_style_set_pad_row(&page_style_, LVD_PADDING);
    lv_style_set_bg_opa(&page_style_, LV_OPA_TRANSP);
}

void DashboardPage::setup(lv_obj_t* parent, int page, LvglItemEventListener *listener) {
    lv_obj_clean(parent);
    this->root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(this->root_);
    lv_obj_set_size(this->root_, lv_pct(100), lv_pct(100));
    ESP_LOGD(TAG, "DashboardPage::setup rows: %d, cols: %d", this->def_->rows, this->def_->cols);
    this->row_dsc_[this->def_->rows] = LV_GRID_TEMPLATE_LAST;
    this->col_dsc_[this->def_->cols] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_style_grid_row_dsc_array(this->root_, this->row_dsc_, 0);
    lv_obj_set_style_grid_column_dsc_array(this->root_, this->col_dsc_, 0);
    lv_obj_set_layout(this->root_, LV_LAYOUT_GRID);
    lv_obj_add_style(this->root_, &page_style_, 0);

    for (int i = 0; i < this->def_->items_size; i++) {
        auto item_def = this->def_->items[i];
        auto* item = DashboardItem::new_instance(&this->def_->items[i]);
        if (item != 0) {
            this->items_.push_back(item);
            item->set_definition(&this->def_->items[i]);
            item->setup(this->root_);
            lv_obj_set_grid_cell(item->get_lv_obj(), 
                LV_GRID_ALIGN_STRETCH, item_def.col, item_def.cols, 
                LV_GRID_ALIGN_STRETCH, item_def.row, item_def.rows
            );
            item->set_listener(page, i, listener);
        }
    }
}

void DashboardPage::destroy() {
    for (auto* item : this->items_) {
        item->destroy();
        free(item);
    }
    this->items_.clear();

    if (this->root_ != 0) {
        lv_obj_del(this->root_);
        this->root_ = 0;
    }
}

void DashboardPage::for_each_item(std::function<void(int, DashboardItem*)> &&fn, int item) {
    for (int i = 0; i < this->items_.size(); i++) {
        if ((item == -1) || (item == i)) fn(i, this->items_[i]);
    }
}


ItemDef default_item_ = {.col = 2, .row = 1, .cols = 1, .rows = 1, .icon = "\U000F0709", .label = "Loading...", .layout = "local"};
PageDef default_page_ = {.cols = 5, .rows = 3, .items = { default_item_ }, .items_size = 1};

void LvglDashboard::init(lv_obj_t* obj) {
    lv_style_init(&root_page_style_);
    lv_style_init(&sub_page_style_);
    lv_style_init(&top_style_);
    lv_style_init(&top_style_collapsed_);
    lv_style_init(&root_btn_style_normal_);
    lv_style_init(&root_btn_style_pressed_);

    lv_style_set_bg_color(&root_page_style_, LVD_BG_COLOR);
    lv_style_set_text_color(&root_page_style_, LVD_TEXT_COLOR);
    lv_style_set_bg_opa(&root_page_style_, LV_OPA_COVER);
    lv_style_set_pad_all(&root_page_style_, LVD_PADDING);

    lv_style_set_bg_color(&sub_page_style_, LVD_BG_COLOR);
    lv_style_set_text_color(&sub_page_style_, LVD_TEXT_COLOR);
    lv_style_set_bg_opa(&sub_page_style_, LV_OPA_COVER);
    lv_style_set_pad_all(&sub_page_style_, LVD_PADDING);

    lv_obj_set_style_pad_all(lv_layer_top(), LVD_PADDING, 0);

    lv_style_set_bg_opa(&top_style_, LV_OPA_TRANSP);
    lv_style_set_radius(&top_style_, LVD_BORDER_RADIUS);
    lv_style_set_pad_column(&top_style_, LVD_PADDING);
    lv_style_set_width(&top_style_, lv_pct(100));
    lv_style_set_height(&top_style_, lv_pct(100));

    lv_style_set_bg_color(&top_style_collapsed_, LVD_PANEL_BG_COLOR);
    lv_style_set_bg_opa(&top_style_collapsed_, LV_OPA_COVER);
    lv_style_set_height(&top_style_collapsed_, 35 + 4 * LVD_PADDING);

    lv_style_set_radius(&root_btn_style_normal_, LVD_BORDER_RADIUS);
    lv_style_set_pad_all(&root_btn_style_normal_, LVD_PADDING);
    lv_style_set_bg_color(&root_btn_style_normal_, LVD_BTN_BG_COLOR);
    lv_style_set_bg_opa(&root_btn_style_normal_, LV_OPA_COVER);

    // lv_style_set_translate_y(&root_btn_style_pressed_, LVD_PRESS_TRANSLATE_Y);
    lv_style_set_bg_color(&root_btn_style_pressed_, LVD_BTN_PRESSED_COLOR);

}

lv_obj_t* LvglDashboard::create_sub_page(lv_obj_t* root) {
    static lv_coord_t row_dsc_[2] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t col_dsc_[3] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_remove_style_all(root);
    lv_obj_set_style_grid_row_dsc_array(root, row_dsc_, 0);
    lv_obj_set_style_grid_column_dsc_array(root, col_dsc_, 0);    
    lv_obj_set_layout(root, LV_LAYOUT_GRID);

    lv_obj_add_style(root, &sub_page_style_, 0);
    this->sub_page_close_btn_ = this->create_root_btn(root, "\U000F004D");
    lv_obj_set_grid_cell(this->sub_page_close_btn_, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_add_flag(this->sub_page_close_btn_, LV_OBJ_FLAG_USER_4);
    subscribe_to_tap_events_(this->sub_page_close_btn_, this);

    auto* dashboard_ = lv_obj_create(root);
    lv_obj_remove_style_all(dashboard_);
    lv_obj_set_size(dashboard_, this->width_, this->height_);
    lv_obj_set_grid_cell(dashboard_, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_START, 0, 1);

    return root;
}

lv_obj_t* LvglDashboard::create_more_page(lv_obj_t* root) {
    static lv_coord_t row_dsc_[2] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t col_dsc_[3] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_remove_style_all(root);
    lv_obj_set_style_grid_row_dsc_array(root, row_dsc_, 0);
    lv_obj_set_style_grid_column_dsc_array(root, col_dsc_, 0);    
    lv_obj_set_layout(root, LV_LAYOUT_GRID);

    lv_obj_add_style(root, &sub_page_style_, 0);
    this->more_page_close_btn_ = this->create_root_btn(root, "\U000F004D");
    lv_obj_set_grid_cell(this->more_page_close_btn_, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    subscribe_to_tap_events_(this->more_page_close_btn_, this);

    auto* dashboard_ = lv_obj_create(root);
    lv_obj_remove_style_all(dashboard_);
    lv_obj_set_size(dashboard_, this->width_, this->height_);
    lv_obj_set_grid_cell(dashboard_, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_START, 0, 1);

    lv_obj_set_layout(dashboard_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(dashboard_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dashboard_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(dashboard_, 2 * LVD_PADDING, 0);

    return root;
}

lv_obj_t* LvglDashboard::create_root_page(lv_obj_t* root) {
    static lv_coord_t row_dsc_[2] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t col_dsc_[3] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_remove_style_all(root);
    lv_obj_set_style_grid_row_dsc_array(root, row_dsc_, 0);
    lv_obj_set_style_grid_column_dsc_array(root, col_dsc_, 0);    
    lv_obj_set_layout(root, LV_LAYOUT_GRID);

    lv_obj_add_style(root, &root_page_style_, 0);
    auto* dashboard_ = lv_obj_create(root);
    lv_obj_remove_style_all(dashboard_);
    lv_obj_set_size(dashboard_, this->width_, this->height_);
    lv_obj_set_grid_cell(dashboard_, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_START, 0, 1);

    lv_obj_add_event_cb(root, lvgl_event_listener_<LvglDashboard>, LV_EVENT_GESTURE, this);

    return root;
}


lv_obj_t* LvglDashboard::create_root_btn(lv_obj_t* root, std::string icon) {
    auto* obj = lv_btn_create(root);
    lv_obj_remove_style_all(obj);
    // lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_add_style(obj, &root_btn_style_normal_, 0);
    lv_obj_add_style(obj, &root_btn_style_pressed_, LV_STATE_PRESSED);

    auto* label = lv_label_create(obj);
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, small_mdi_font->get_lv_font(), 0);
    lv_obj_set_style_text_color(label, LVD_TEXT_COLOR, 0);
    lv_label_set_text(label, icon.c_str());
    return obj;
}

lv_obj_t* LvglDashboard::create_buttons(lv_obj_t* root) {
    auto* buttons_ = lv_obj_create(root);
    lv_obj_remove_style_all(buttons_);
    lv_obj_add_style(buttons_, &top_style_, 0);
    lv_obj_add_style(buttons_, &top_style_collapsed_, LV_STATE_USER_1);
    lv_obj_set_align(buttons_, LV_ALIGN_BOTTOM_LEFT);

    lv_obj_set_style_grid_row_dsc_array(buttons_, this->btns_row_dsc, 0);
    lv_obj_set_style_grid_column_dsc_array(buttons_, this->btns_row_dsc, 0);
    
    lv_obj_set_layout(buttons_, LV_LAYOUT_GRID);

    this->dashboard_btn_ = this->create_root_btn(buttons_, "\U000F056E");
    lv_obj_set_style_bg_color(this->dashboard_btn_, LVD_SWITCH_ON_LINE_COLOR, LV_STATE_USER_1);
    lv_obj_add_flag(this->dashboard_btn_, LV_OBJ_FLAG_HIDDEN);
    subscribe_to_tap_events_(this->dashboard_btn_, this);
    return buttons_;
}

void LvglDashboard::setup() {
 
    this->page_ = lv_scr_act();
    // ESP_LOGD(TAG, "Getting current page root: %d", (this->page_ != 0));

    // Cleanup
    lv_obj_clean(this->page_);
    lv_obj_remove_style_all(this->page_);

    // ESP_LOGD(TAG, "Getting current page root: %d, %d", lv_obj_get_width(this->page_), lv_obj_get_x(this->page_));

    // Init theme
    auto* normal_font_ = this->normal_font_ != 0? this->normal_font_->get_lv_font(): &lv_font_montserrat_14;
    this->theme_ = lv_theme_default_init(
        this->root_->get_disp(), 
        LVD_SWITCH_LINE_COLOR, LVD_SWITCH_PRESSED_LINE_COLOR, 
        true, 
        normal_font_
    );
    this->theme_->font_large = this->large_font_ != 0? this->large_font_->get_lv_font(): &lv_font_montserrat_28;
    this->theme_->font_small = &lv_font_montserrat_12;
    lv_disp_set_theme(this->root_->get_disp(), this->theme_);

    this->init(this->page_);
    DashboardButton::init(this->page_);
    DashboardPage::init(this->page_);
    DashboardItem::init(this->page_);
    MoreInfoPage::init(this->page_);

    this->page_ = this->create_root_page(this->page_);
    this->sub_page_ = this->create_sub_page(lv_obj_create(NULL));
    this->more_page_ = this->create_more_page(lv_obj_create(NULL));
    this->buttons_ = this->create_buttons(lv_layer_top());
    this->more_info_page_ = new MoreInfoPage();
    this->more_info_page_->set_change_listener([this](std::string entity_id, std::string id, int value) {
        this->send_event_("change", [&entity_id, &id, &value] (esphome::api::HomeassistantServiceResponse* resp) {
            esphome::api::HomeassistantServiceMap entry_;
            entry_.key = "id";
            entry_.value = entity_id;
            resp->data.push_back(entry_);

            esphome::api::HomeassistantServiceMap entry__;
            entry__.key = "op";
            entry__.value = id;
            resp->data.push_back(entry__);

            esphome::api::HomeassistantServiceMap entry___;
            entry___.key = "value";
            entry___.value = std::to_string(value);
            resp->data.push_back(entry___);
        });
    });
    this->more_info_page_->set_data_request_listener([this](std::string entity_id, std::string id) {
        this->send_event_("data_request", [&entity_id, &id] (esphome::api::HomeassistantServiceResponse* resp) {
            esphome::api::HomeassistantServiceMap entry_;
            entry_.key = "id";
            entry_.value = entity_id;
            resp->data.push_back(entry_);

            esphome::api::HomeassistantServiceMap entry__;
            entry__.key = "op";
            entry__.value = id;
            resp->data.push_back(entry__);
        });
    });

    this->set_buttons();
    this->set_pages(&default_page_, 1);
    this->render_page(0);
}

bool LvglDashboard::buttons_visible() {
    return this->has_buttons() && (!lv_obj_has_state(this->buttons_, LV_STATE_USER_1));
}

bool LvglDashboard::more_page_visible() {
    return (lv_disp_get_scr_act(NULL) == this->more_page_);
}

void LvglDashboard::show_buttons(bool visible) {
    this->cancel_timeout("show_buttons_");
    if (!this->has_buttons()) {
        lv_obj_add_flag(this->buttons_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (visible) {
        lv_obj_clear_state(this->buttons_, LV_STATE_USER_1);
        lv_obj_clear_state(this->dashboard_btn_, LV_STATE_USER_1);
    } else {
        lv_obj_add_state(this->buttons_, LV_STATE_USER_1);
        lv_obj_add_state(this->dashboard_btn_, LV_STATE_USER_1);
        if (this->dashboard_timeout_ > 0) {
            this->set_timeout("show_buttons_", 1000 * this->dashboard_timeout_, [this]() {
                if (!this->more_page_visible())
                    this->show_buttons(true);
            });
        }
    }
}

void LvglDashboard::set_buttons() {
    for (auto* item: this->button_objs_) {
        item->destroy();
        free(item);
    }
    this->button_objs_.clear();
    lv_obj_add_flag(this->dashboard_btn_, LV_OBJ_FLAG_HIDDEN);
    auto size = this->switches_.size();
    int cells = size > 0? size + 1: 2;
    int dashboard_cell = 0;
    for (int i = 0; i < cells; i++) { this->btns_col_dsc[i] = LV_GRID_FR(1); }
    this->btns_col_dsc[dashboard_cell] = LV_GRID_CONTENT;
    this->btns_col_dsc[cells] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_style_grid_column_dsc_array(this->buttons_, this->btns_col_dsc, 0);
    for (int i = 0; i < size; i++) {
        auto* btn = new DashboardButton();
        btn->set_listener(i, this);
        btn->setup(this->buttons_, this->switches_[i]);
        this->button_objs_.push_back(btn);
        lv_obj_set_grid_cell(
            btn->get_lv_obj(), 
            LV_GRID_ALIGN_STRETCH, i < dashboard_cell? i: i+1, 1, 
            LV_GRID_ALIGN_STRETCH, 0, 1
        );
    }
    if (size > 0) {
        lv_obj_clear_flag(this->dashboard_btn_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_grid_cell(this->dashboard_btn_, LV_GRID_ALIGN_START, dashboard_cell, 1, LV_GRID_ALIGN_END, 0, 1);
    }
    this->show_buttons(this->dashboard_timeout_ > 0);
}

void LvglDashboard::send_more_page_event(bool visible) {
    this->send_event_("more", [this, &visible](esphome::api::HomeassistantServiceResponse* resp) {
        esphome::api::HomeassistantServiceMap entry_;
        entry_.key = "visible";
        entry_.value = visible? "1": "0";
        resp->data.push_back(entry_);
    });
}

void LvglDashboard::show_more_page(JsonObject data) {
    this->more_info_page_->destroy();
    this->more_info_page_->setup(lv_obj_get_child(this->more_page_, 1), data);
    lv_disp_load_scr(this->more_page_);
    this->show_buttons(false);
    this->send_more_page_event(true);
}

void LvglDashboard::hide_more_page() {
    this->more_info_page_->destroy();
    this->show_page(this->page_no_);
    this->show_buttons(false); // Will trigger auto-show
    this->send_more_page_event(false);
}


void LvglDashboard::set_pages(PageDef* pages, int size) {
    this->page_no_ = 0; // To avoid double destroy
    for (auto* item: this->page_objs_) {
        item->destroy();
        free(item);
    }
    this->page_objs_.clear();

    for (int i = 0; i < size; i++) {
        auto* page = new DashboardPage(&pages[i]);
        this->page_objs_.push_back(page);
    }
}

void LvglDashboard::show_page(int index) {
    if (this->page_no_ > 0) {
        // Sub-page was shown
        this->page_objs_[this->page_no_]->destroy();
    }
    if (index == 0) {
        // Just show
        lv_disp_load_scr(this->page_);
    } else {
        this->render_page(index); // Clean, render, show
    }
    this->send_event(index, -1, "page");
    this->page_no_ = index;
}

void LvglDashboard::render_page(int index) {
    auto* root = this->page_;
    auto* dashboard = lv_obj_get_child(root, 0);

    if (index > 0) {
        root = this->sub_page_;
        dashboard = lv_obj_get_child(root, 1);
    }
    // lv_obj_clean(dashboard);

    this->page_objs_[index]->setup(dashboard, index, this);
    lv_disp_load_scr(root);
}

void LvglDashboard::hide_page(int index) {

}

void LvglDashboard::set_mdi_fonts(esphome::font::Font* small_font, esphome::font::Font* large_font) {
    small_mdi_font = new esphome::lvgl::FontEngine(small_font);
    large_mdi_font = new esphome::lvgl::FontEngine(large_font);
    ESP_LOGD(TAG, "Font params: %d, %d", large_mdi_font->get_lv_font()->line_height, small_mdi_font->get_lv_font()->line_height);
}

void LvglDashboard::service_set_pages(std::vector<std::string> pages, int page) {
    icons_->clear();
    int pages_size = 0;
    for (auto json_ : pages) {
        if (json_parse_(json_, [&pages_size, this](JsonObject obj) {
            // Make PageDef
            int pcols = obj["cols"];
            int prows = obj["rows"];
            PageDef page = {.cols = pcols, .rows = prows};
            JsonArray items = obj["items"];
            int items_size = 0;
            int col = 0;
            int row = 0;
            for (JsonObject item : items) {
                col = item.containsKey("col")? item["col"]: col;
                row = item.containsKey("row")? item["row"]: row;
                int cols = item.containsKey("cols")? item["cols"]: 1;
                int rows = item.containsKey("rows")? item["rows"]: 1;
                ItemDef item_ = {.col = col, .row = row, .cols = cols, .rows = rows, .layout = item["layout"]};
                page.items[items_size++] = item_;
                col += cols;
                if (col >= pcols) {
                    row++;
                    col = 0;
                }
            }
            page.items_size = items_size;
            this->pages_[pages_size] = page;
        })) {
            pages_size++;
        }
    }
    this->set_pages((PageDef*)&this->pages_, pages_size);
    this->render_page(0);
    this->send_event(0, -1, "page");
    this->send_more_page_event(false);
}

void LvglDashboard::service_set_value(int page, int item, std::string value) {
    json_parse_(value, [this, &page, &item](JsonObject obj) {
        this->for_each_item([&obj](int, DashboardPage*, int, DashboardItem* item) {
            if (obj.containsKey("_h")) {
                bool hidden = obj["_h"];
                if (hidden) {
                    lv_obj_add_flag(item->get_lv_obj(), LV_OBJ_FLAG_HIDDEN);
                    return;
                }
            }
            lv_obj_clear_flag(item->get_lv_obj(), LV_OBJ_FLAG_HIDDEN);
            item->set_value(obj);
        }, page, item);
    });
}

void LvglDashboard::service_set_data(int page, int item, int32_t* data, int size, int offset, int total_size) {
    this->for_each_item([data, &size, &offset, &total_size](int, DashboardPage*, int, DashboardItem* item) {
        item->set_data(data, size, offset, total_size);
    }, page, item);
}

void LvglDashboard::for_each_page(std::function<void(int, DashboardPage*)> &&fn, int page) {
    for (int i = 0; i < this->page_objs_.size(); i++) {
        if ((page == -1) || (page == i)) fn(i, this->page_objs_[i]);
    }
}

void LvglDashboard::for_each_item(std::function<void(int, DashboardPage*, int, DashboardItem*)> &&fn, int page, int item) {
    this->for_each_page([&fn, &item](int page_no, DashboardPage* page) {
        page->for_each_item([&page_no, page, &fn](int item_no, DashboardItem* item) {
            fn(page_no, page, item_no, item);
        }, item);
    }, page);
}

void LvglDashboard::loop() {
    this->for_each_item([](int, DashboardPage*, int, DashboardItem* item) {
        item->loop();
    }, -1, -1);
}

void LvglDashboard::send_event_(std::string type, std::function<void(esphome::api::HomeassistantServiceResponse*)> &&fn) {
    esphome::api::HomeassistantServiceResponse resp;
    esphome::api::HomeassistantServiceMap entry_;
    entry_.key = "type";
    entry_.value = type;
    resp.data.push_back(entry_);

    fn(&resp);

    resp.service = "esphome.lvgl_dashboard_event";
    resp.is_event = true;
    this->api_server_->send_homeassistant_service_call(resp);
}

void LvglDashboard::send_event(int page, int item, std::string type) {
    this->send_event_(type, [&page, &item] (esphome::api::HomeassistantServiceResponse* resp) {
        if (page != -1) {
            esphome::api::HomeassistantServiceMap entry_;
            entry_.key = "page";
            entry_.value = std::to_string(page);
            resp->data.push_back(entry_);
        }
        if (item != -1) {
            esphome::api::HomeassistantServiceMap entry_;
            entry_.key = "item";
            entry_.value = std::to_string(item);
            resp->data.push_back(entry_);
        }
    });
}

static std::map<int, std::string> event_type_map_ = {{LV_EVENT_SHORT_CLICKED, "click"}, {LV_EVENT_LONG_PRESSED, "long_press"}};

bool LvglDashboard::turn_backlight() {
    if ((this->backlight_ != 0) && (!this->backlight_->state)) {
        // Backlight is OFF - turn it first
        this->backlight_->turn_on();
        return true;
    }
    return false;
}

void LvglDashboard::on_item_event(int page, int item, int event) {
    ESP_LOGD(TAG, "LvglDashboard::on_item_event: %d, %d, %d", page, item, event);
    if (this->turn_backlight()) {
        return;
    }
    this->show_buttons(false);
    if (auto search = event_type_map_.find(event); search != event_type_map_.end()) 
        this->send_event(page, item, search->second);
}

void LvglDashboard::on_data_request(int page, int item) {
    this->send_event(page, item, "data_request");
}

void LvglDashboard::on_tap_event(lv_event_code_t code, lv_event_t* event) {
    if (this->turn_backlight()) {
        return;
    }
    if (lv_event_get_target(event) == this->sub_page_close_btn_) {
        ESP_LOGD(TAG, "LvglDashboard::on_event: back_button click");
        this->show_page(0);
    }
    if (lv_event_get_target(event) == this->more_page_close_btn_) {
        ESP_LOGD(TAG, "LvglDashboard::on_event: more_page_close_btn click");
        this->hide_more_page();
    }
    if (lv_event_get_target(event) == this->dashboard_btn_) {
        ESP_LOGD(TAG, "LvglDashboard::on_event: dashboard_btn_ click %d", this->buttons_visible());
        this->show_buttons(!this->buttons_visible());
    }
}

void LvglDashboard::on_event(lv_event_t* event) {
    ESP_LOGD(TAG, "LvglDashboard::on_event: %d", event->code);
    if (event->code == LV_EVENT_GESTURE) {
        auto dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        ESP_LOGD(TAG, "LvglDashboard::on_event: gesture %d", dir);
    }
}

void LvglDashboard::service_show_page(int page) {
    this->show_page(page);
    this->send_more_page_event(false);
}

void LvglDashboard::service_set_button(int index, std::string json_value) {
    json_parse_(json_value, [this, &index](JsonObject obj) {
        if ((index >= 0) && (index < this->button_objs_.size())) {
            this->button_objs_[index]->set_value(obj);
        }
    });
}

bool LvglDashboard::on_button(int index, lv_event_code_t code) {
    if (this->turn_backlight()) {
        return true;
    }
    if (code == LV_EVENT_SHORT_CLICKED) {
        this->send_event(-1, index, "button");
        return false;
    }
    if (code == LV_EVENT_LONG_PRESSED) {
        this->send_event(-1, index, "long_button");
        return false;
    }
    return false;
}

void LvglDashboard::service_hide_more() {
    this->hide_more_page();
}

void LvglDashboard::service_show_more(std::string json_value) {
    json_parse_(json_value, [this](JsonObject obj) {
        this->show_more_page(obj);
    });
}

void LvglDashboard::service_set_data_more(int32_t* data, int size, int offset, int total_size) {
    if (this->more_page_visible()) {
        this->more_info_page_->set_data(data, size, offset, total_size);
    }
}

void LvglDashboard::service_play_rtttl(std::string song) {
    if (this->rtttl_ != 0) {
        this->rtttl_->play(song);
    }
}

void MoreInfoPage::init(lv_obj_t* obj) {
    lv_style_init(&more_page_base_);
    lv_style_init(&more_page_slider_);
    lv_style_init(&more_page_slider_ind_);
    lv_style_init(&more_page_slider_knob_);

    lv_style_init(&more_page_switch_);
    lv_style_init(&more_page_switch_ind_);
    lv_style_init(&more_page_switch_ind_checked_);
    lv_style_init(&more_page_switch_knob_);
    lv_style_init(&more_page_switch_knob_checked_);
    lv_style_init(&more_page_image_);

    lv_style_set_width(&more_page_base_, lv_pct(100));
    lv_style_set_max_width(&more_page_base_, lv_pct(70));
    lv_style_set_height(&more_page_base_, 100);
    lv_style_set_radius(&more_page_base_, LVD_BORDER_RADIUS);
    lv_style_set_bg_color(&more_page_base_, LVD_PANEL_BG_COLOR);
    lv_style_set_anim_time(&more_page_base_, 0);

    lv_style_set_radius(&more_page_switch_ind_, LVD_BORDER_RADIUS);
    lv_style_set_bg_color(&more_page_switch_ind_, LVD_PANEL_BG_COLOR);
    lv_style_set_radius(&more_page_switch_ind_checked_, LVD_BORDER_RADIUS);
    lv_style_set_bg_color(&more_page_switch_ind_checked_, lv_color_lighten(LVD_BTN_ON_COLOR, LV_OPA_50));

    lv_style_set_radius(&more_page_switch_knob_, LVD_BORDER_RADIUS);
    lv_style_set_bg_color(&more_page_switch_knob_, LVD_BTN_BG_COLOR);
    lv_style_set_bg_color(&more_page_switch_knob_checked_, LVD_BTN_ON_COLOR);

    lv_style_set_radius(&more_page_slider_ind_, LVD_BORDER_RADIUS);
    lv_style_set_radius(&more_page_slider_knob_, LVD_BORDER_RADIUS);
    lv_style_set_width(&more_page_slider_knob_, LVD_PADDING);

    lv_style_set_bg_color(&more_page_slider_ind_, lv_color_lighten(LVD_BTN_ON_COLOR, LV_OPA_50));
    lv_style_set_bg_color(&more_page_slider_knob_, LVD_BTN_ON_COLOR);

    lv_style_set_transform_width(&more_page_slider_knob_, -40);
    lv_style_set_transform_height(&more_page_slider_knob_, -4);

    lv_style_set_radius(&more_page_image_, LVD_BORDER_RADIUS);
    lv_style_set_bg_color(&more_page_image_, LVD_PANEL_BG_COLOR);
    lv_style_set_pad_all(&more_page_image_, LVD_PADDING);
    lv_style_set_width(&more_page_image_, LV_SIZE_CONTENT);
    lv_style_set_height(&more_page_image_, LV_SIZE_CONTENT);
}

void MoreInfoPage::setup(lv_obj_t* parent, JsonObject data) {
    this->entity_id_ = (std::string)data["id"];
    std::string title = data["title"];
    this->ids_.clear();
    JsonArray features = data["features"];
    lv_obj_clean(parent);

    auto* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, lv_theme_get_font_normal(parent), 0);
    lv_label_set_text(label, title.c_str());
    lv_obj_set_style_text_color(label, LVD_TEXT_COLOR, 0);

    ESP_LOGD(TAG, "LvglDashboard::show_more_page features: %u", features.size());
    for (int i = 0; i < features.size(); i++) {
        JsonObject item = features[i];
        if (item["type"] == "slider") {
            this->ids_.push_back(item["id"]);
            auto* cmp = lv_slider_create(parent);
            lv_slider_set_range(cmp, item["min"], item["max"]);
            lv_obj_add_style(cmp, &more_page_base_, 0);
            lv_obj_add_style(cmp, &more_page_slider_ind_, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_add_style(cmp, &more_page_slider_knob_, LV_PART_KNOB | LV_STATE_DEFAULT);
            std::string icon_str = "\U000F00DF"; // brightness
            if (item["id"] == "value") {
                lv_obj_set_style_bg_color(cmp, lv_color_lighten(lv_theme_get_color_secondary(cmp), LV_OPA_50), LV_PART_INDICATOR);
                lv_obj_set_style_bg_color(cmp, lv_theme_get_color_secondary(cmp), LV_PART_KNOB);
                icon_str = "\U000F03A0"; // 123
            }
            auto* icon = lv_label_create(cmp);
            lv_obj_set_style_text_font(icon, small_mdi_font->get_lv_font(), 0);
            lv_label_set_text(icon, icon_str.c_str());
            lv_obj_set_style_text_color(icon, LVD_TEXT_COLOR, 0);
            lv_obj_center(icon);
            lv_obj_add_event_cb(cmp, lvgl_event_listener_<MoreInfoPage>, LV_EVENT_VALUE_CHANGED, this);
        }
        if (item["type"] == "toggle") {
            auto* cmp = lv_switch_create(parent);
            this->ids_.push_back(item["id"]);
            if (item["value"] == true) {
                lv_obj_add_state(cmp, LV_STATE_CHECKED);
            }
            auto* icon = lv_label_create(cmp);
            lv_obj_set_style_text_font(icon, small_mdi_font->get_lv_font(), 0);
            lv_label_set_text(icon, "\U000F0425"); // toggle
            lv_obj_set_style_text_color(icon, LVD_TEXT_COLOR, 0);
            lv_obj_center(icon);
            lv_obj_add_style(cmp, &more_page_base_, 0);
            lv_obj_add_style(cmp, &more_page_switch_ind_, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_add_style(cmp, &more_page_switch_ind_checked_, LV_PART_INDICATOR | LV_STATE_CHECKED);
            lv_obj_add_style(cmp, &more_page_switch_knob_, LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_add_style(cmp, &more_page_switch_knob_checked_, LV_PART_KNOB | LV_STATE_CHECKED);
            lv_obj_add_event_cb(cmp, lvgl_event_listener_<MoreInfoPage>, LV_EVENT_VALUE_CHANGED, this);
        }
        if (item["type"] == "image") {
            auto* cmp = lv_obj_create(parent);
            lv_obj_add_style(cmp, &more_page_image_, 0);
            this->image_cmp_ = lv_img_create(cmp);
            this->image_.header.always_zero = 0;
            this->image_.header.w = item["width"];
            this->image_.header.h = item["height"];
            this->image_.header.cf = LV_IMG_CF_TRUE_COLOR;
            if (this->data_request_listener_ != 0)
                this->data_request_listener_(this->entity_id_, item["id"]);
        }
    }
}

void MoreInfoPage::destroy() {
    WithDataBuffer::destroy_();
    this->image_cmp_ = 0;
}

void MoreInfoPage::on_event(lv_event_t* event) {
    auto* it = lv_event_get_target(event);
    auto index = lv_obj_get_index(it) - 1; // minus title
    auto code = lv_event_get_code(event);
    if (code == LV_EVENT_VALUE_CHANGED) {
        std::string id = this->ids_[index];
        int value = 0;
        if (id == "toggle") {
            value = lv_obj_has_state(it, LV_STATE_CHECKED)? 1: 0;
        }
        if ((id == "bri") || (id == "value")) {
            value = lv_slider_get_value(it);
        }
        if (this->change_listener_ != 0) {
            this->change_listener_(this->entity_id_, id, value);
        }
    }

}
void MoreInfoPage::on_tap_event(lv_event_code_t code, lv_event_t* event) {

}

void MoreInfoPage::set_data(int32_t* data, int size, int offset, int total_size) {
    if (this->set_data_(data, size, offset, total_size) && (this->image_cmp_ != 0)) {
        this->image_.data_size = this->data_size_;
        this->image_.data = (unsigned char*)this->data_;
        lv_img_set_src(this->image_cmp_, &this->image_);
    }
}


}
}
