#pragma once

#include <vector>
#include <map>
#include <set>

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/rtttl/rtttl.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
namespace esphome {
namespace lvgl_dashboard {

#ifdef USE_BINARY_SENSOR
class DashboardComponent : public esphome::Component, public esphome::binary_sensor::BinarySensorInitiallyOff {
    protected:
        esphome::Component* component_ = 0;

    public:
        void setup() override {
            ESP_LOGI("lvgl_dashboard", "DashboardComponent::setup(): %d", this->component_->is_failed());
            this->publish_state(this->component_->is_failed());
        }
        float get_setup_priority() const override { return esphome::setup_priority::BEFORE_CONNECTION; }
        void set_component(esphome::Component* component) { this->component_ = component; }
};
#endif

#ifndef LVD_BG_COLOR
    #define LVD_BG_COLOR lv_color_black()
#endif
#ifndef LVD_TEXT_COLOR
    #define LVD_TEXT_COLOR lv_color_white()
#endif
#ifndef LVD_TEXT_ON_COLOR
    #define LVD_TEXT_ON_COLOR lv_color_black()
#endif
#ifndef LVD_PADDING
    #define LVD_PADDING 7
#endif
#ifndef LVD_BORDER_RADIUS
    #define LVD_BORDER_RADIUS 7
#endif
#ifndef LVD_LAYOUT_GAP
    #define LVD_LAYOUT_GAP 3
#endif
#ifndef LVD_PANEL_BG_COLOR
    #define LVD_PANEL_BG_COLOR lv_palette_darken(LV_PALETTE_GREY, 4)
#endif
#ifndef LVD_BTN_BG_COLOR
    #define LVD_BTN_BG_COLOR lv_palette_darken(LV_PALETTE_GREY, 3)
#endif
#ifndef LVD_BTN_PRESSED_COLOR
    #define LVD_BTN_PRESSED_COLOR lv_palette_darken(LV_PALETTE_GREY, 2)
#endif
#ifndef LVD_BTN_ON_COLOR
    #define LVD_BTN_ON_COLOR lv_palette_main(LV_PALETTE_YELLOW)
#endif
#ifndef LVD_SWITCH_LINE_COLOR
    #define LVD_SWITCH_LINE_COLOR lv_palette_main(LV_PALETTE_TEAL)
#endif
#ifndef LVD_SWITCH_PRESSED_LINE_COLOR
    #define LVD_SWITCH_PRESSED_LINE_COLOR lv_palette_main(LV_PALETTE_BLUE)
#endif
#ifndef LVD_SWITCH_LONG_PRESSED_LINE_COLOR
    #define LVD_SWITCH_LONG_PRESSED_LINE_COLOR lv_palette_main(LV_PALETTE_RED)
#endif
#ifndef LVD_SWITCH_ON_LINE_COLOR
    #define LVD_SWITCH_ON_LINE_COLOR lv_palette_main(LV_PALETTE_ORANGE)
#endif
#ifndef LVD_SWITCH_LINE_HEIGHT
    #define LVD_SWITCH_LINE_HEIGHT 7
#endif
#ifndef LVD_SWITCH_HEIGHT_HOR
    #define LVD_SWITCH_HEIGHT_HOR 82
#endif
#ifndef LVD_SWITCH_HEIGHT_VERT
    #define LVD_SWITCH_HEIGHT_VERT 45
#endif
#ifndef LVD_SWITCH_CLICK_RTTTL
    #define LVD_SWITCH_CLICK_RTTTL "click:d=64,o=4,b=120:d"
#endif
#ifndef LVD_SWITCH_LONG_CLICK_RTTTL
    #define LVD_SWITCH_LONG_CLICK_RTTTL "click:d=64,o=4,b=120:a"
#endif
#ifndef LVD_CONNECT_LINE_HEIGHT
    #define LVD_CONNECT_LINE_HEIGHT 2
#endif
#ifndef LVD_CONNECT_LINE_COLOR
    #define LVD_CONNECT_LINE_COLOR lv_palette_main(LV_PALETTE_RED)
#endif

typedef struct {
    lv_coord_t width;
    lv_coord_t height;
    bool vertical;

    lv_color_t text_color;
    lv_color_t bg_color;
    lv_color_t text_on_color;
    lv_color_t panel_bg_color;
    lv_color_t btn_bg_color;
    lv_color_t btn_pressed_color;
    lv_color_t btn_on_color;
    lv_color_t switch_line_color;
    lv_color_t switch_pressed_line_color;
    lv_color_t switch_long_pressed_line_color;
    lv_color_t switch_on_line_color;
    lv_coord_t switch_line_height;
    lv_coord_t switch_height;
    lv_coord_t padding;
    lv_coord_t radius;
    lv_coord_t layout_gap;
} ThemeDef;

typedef struct {
    std::string id;
    int col;
    int row;
    int cols;
    int rows;
    std::string icon;
    std::string label;
    std::string layout;
} ItemDef;

typedef struct {
    std::string title;
    int cols;
    int rows;
    bool vertical;
    ItemDef items[32];
    int items_size;
} PageDef;

class ButtonComponentWrapper {
    protected:
        esphome::EntityBase* component_ = 0;
        std::string type_;

        template <typename T>
        T* as_t() { return (T*)this->component_; }
    public:
        bool is_on();
        void toggle();
        void add_on_state_callback(std::function<void(bool)> &&callback);
        void set_component(std::string type, esphome::EntityBase* component) {
            this->component_ = component;
            this->type_ = type;
        }
};

class LvglItemEventListener {
    public:
        virtual void on_item_event(int page, int item, int event) = 0;
        virtual void on_data_request(int page, int item) = 0;
};

class LvglPageEventListener {
    public:
        virtual void on_back_button(int page) = 0;
};

class DashboardButtonListener {
    public:
        virtual bool on_button(int index, lv_event_code_t event) = 0;
};

typedef struct {
    int item;
    int page;
    LvglItemEventListener* listener;
} ItemEventListenerDef;

typedef struct {
    int index;
    DashboardButtonListener* listener;
} DashboardButtonListenerDef;

typedef struct {
    int index;
    LvglPageEventListener* listener;
} LvglPageEventListenerDef;

class MdiFont {
    protected:
        int size_;
        lv_font_t lv_font_ {};
        std::map<uint32_t, uint8_t*> glyphs_ = {};
        std::map<std::string, uint32_t> codes_ = {};

    public:
        MdiFont(int size);
        const lv_font_t *get_lv_font() { return &this->lv_font_; }

        bool create_glyph_dsc(uint32_t unicode_letter, lv_font_glyph_dsc_t *dsc);
        const uint8_t* get_glyph_data(uint32_t unicode_letter);

        uint32_t add_glyph(std::string icon, std::string b64_data);
        void destroy();

};

class MdiFontCapable {
    protected:
        std::map<int, MdiFont*> fonts_ {};

        MdiFont* get_font(int size);

    public:
        void set_icon(lv_obj_t* obj, JsonObject icon_data);
        void clear();
};

class WithDataBuffer {
    protected:
        uint8_t* data_ = 0;
        uint32_t data_size_ = 0;

        uint8_t* create_data_(uint32_t size);
        bool set_data_(int32_t* data, int size, int offset, int total_size);
        void destroy_();

};

static lv_style_t item_style_normal_;
static lv_style_t item_style_pressed_;
class DashboardItem {

    protected:
        lv_obj_t* root_ = 0;
        ItemDef* def_ = 0;
        bool visible_ = false;

        ItemEventListenerDef listener_{.item = 0, .page = 0, .listener = 0};

        lv_coord_t row_dsc_[4] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        lv_coord_t col_dsc_[4] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

        void set_bg_color(lv_obj_t* obj, JsonObject data);
        void set_text_color(lv_obj_t* obj, JsonObject data);

        void request_data();

    public:
        void set_definition(ItemDef* def) { this->def_ = def; }
        static void init(lv_obj_t* obj, bool init);
        lv_obj_t* get_lv_obj() { return this->root_; }

        virtual void setup(lv_obj_t* root);
        virtual void destroy();

        virtual void set_value(JsonObject data) {}
        virtual void set_data(int32_t* data, int size, int offset, int total_size) {}

        void loop();
        void on_tap_event(lv_event_code_t code, lv_event_t* event);

        void set_listener(int page, int item, LvglItemEventListener* listener);
        virtual bool show(bool visible) {
            bool result = visible != this->visible_;
            this->visible_ = visible; 
            return result;
        }

        static DashboardItem* new_instance(ItemDef* def);
};

class LocalItem : public DashboardItem {
    public:
        void setup(lv_obj_t* root) override;
};

class ButtonItem : public DashboardItem {
    public:
        void setup(lv_obj_t* root) override;
        void set_value(JsonObject data) override;
};

class LayoutItem : public DashboardItem {
    protected:
        lv_coord_t row_dsc_[6] = {
            LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), 
            LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        lv_coord_t col_dsc_[6] = {
            LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), 
            LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    public:
        void setup(lv_obj_t* root) override;
        void set_value(JsonObject data) override;
};

class SensorItem : public DashboardItem {
    public:
        void setup(lv_obj_t* root) override;
        void set_value(JsonObject data) override;
};

class ImageItem : public DashboardItem, public WithDataBuffer {
    protected:
        lv_img_dsc_t image_{};
        lv_obj_t* lv_img_ = 0;
        bool data_pending_ = false;
    public:
        void setup(lv_obj_t* root) override;
        void set_value(JsonObject data) override;
        void set_data(int32_t* data, int size, int offset, int total_size) override;
        void destroy() override;

        void draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t color);
        void show();
        bool show(bool visible) override;
};

static lv_style_t btn_style_normal_;
static lv_style_t btn_wrapper_style_normal_;
static lv_style_t btn_wrapper_style_pressed_;
static lv_style_t btn_line_style_normal_;
static lv_style_t btn_line_style_checked_;
static lv_style_t btn_line_style_pressed_;
static lv_style_t btn_line_style_long_pressed_;
class DashboardButton {
    protected:
        lv_obj_t* root_ = 0;
        lv_obj_t* wrapper_ = 0;
        lv_obj_t* line_ = 0;
        ButtonComponentWrapper* component_ = 0;

        DashboardButtonListenerDef listener_{.listener = 0};

        void set_side_icon(lv_obj_t* obj, JsonObject data);

    public:
        std::string click_rtttl = LVD_SWITCH_CLICK_RTTTL;
        std::string long_click_rtttl = LVD_SWITCH_LONG_CLICK_RTTTL;

        static void init(lv_obj_t* obj, bool init);
        void setup(lv_obj_t* root, ButtonComponentWrapper* component_);
        void destroy();
        lv_obj_t* get_lv_obj() { return this->root_; }

        void on_event(lv_event_t* event);
        void on_tap_event(lv_event_code_t code, lv_event_t* event);
        void set_listener(int index, DashboardButtonListener* listener) {
            this->listener_.index = index;
            this->listener_.listener = listener;
        }
        void set_value(JsonObject data);
        void update_state(bool state);
};

static lv_style_t page_style_;
static lv_style_t sub_page_style_;
class DashboardPage {
    protected:
        PageDef* def_ = 0;
        lv_obj_t* page_ = 0;
        lv_obj_t* root_ = 0; // Dashboard
        lv_obj_t* close_btn_ = 0;

        LvglPageEventListenerDef listener_{.index = 0, .listener = 0};

        std::vector<DashboardItem*> items_ = {};

        lv_coord_t page_row_dsc_[2] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        lv_coord_t page_col_dsc_[3] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

        lv_coord_t row_dsc_[9] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), 
                                 LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        lv_coord_t col_dsc_[9] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), 
                                 LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

        lv_obj_t* create_page(lv_obj_t* root, bool sub_page);

    public:
        DashboardPage(PageDef* def) { this->def_ = def; }
        static void init(lv_obj_t* obj, bool init);
        void setup(lv_obj_t* parent, int page, LvglItemEventListener *listener, LvglPageEventListener *page_listener);
        void destroy(int page);
        void show(int page, bool visible);
        lv_obj_t* get_lv_obj() { return this->root_; }

        void for_each_item(std::function<void(int, DashboardItem*)> &&fn, int item);
        void on_tap_event(lv_event_code_t code, lv_event_t* event);
};

static lv_style_t more_page_base_;
static lv_style_t more_page_slider_;
static lv_style_t more_page_slider_ind_;
static lv_style_t more_page_slider_knob_;
static lv_style_t more_page_switch_;
static lv_style_t more_page_switch_ind_;
static lv_style_t more_page_switch_ind_checked_;
static lv_style_t more_page_switch_knob_;
static lv_style_t more_page_switch_knob_checked_;
static lv_style_t more_page_image_;
class MoreInfoPage : public WithDataBuffer {
    protected:
        std::function<void(std::string, std::string, int)> change_listener_ = 0;
        std::function<void(std::string, std::string)> data_request_listener_ = 0;
        std::function<void()> load_finished_listener_ = 0;
        std::vector<std::string> ids_ {};
        std::string entity_id_;

        lv_img_dsc_t image_{};
        lv_obj_t *image_cmp_ = 0;
        bool immediate_display_ = false;

    public:
        static void init(lv_obj_t* obj, bool init);

        bool setup(lv_obj_t* parent, JsonObject data);
        void destroy();

        void set_change_listener(std::function<void(std::string, std::string, int)> &&listener) {
            this->change_listener_ = listener;
        }

        void set_data_request_listener(std::function<void(std::string, std::string)> &&listener) {
            this->data_request_listener_ = listener;
        }

        void set_load_finished_listener(std::function<void()> &&listener) {
            this->load_finished_listener_ = listener;
        }

        void on_event(lv_event_t* event);
        void on_tap_event(lv_event_code_t code, lv_event_t* event);

        void set_data(int32_t* data, int size, int offset, int total_size);
};

static lv_style_t top_style_;
static lv_style_t top_style_collapsed_;
static lv_style_t root_btn_style_normal_;
static lv_style_t root_btn_style_pressed_;
class LvglDashboard : virtual public LvglItemEventListener, virtual public LvglPageEventListener, public DashboardButtonListener, public esphome::PollingComponent  {
    private:

        lv_coord_t btns_row_dsc[2] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        lv_coord_t btns_col_dsc[5] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

        esphome::lvgl::LvglComponent* root_ = 0;
        esphome::api::APIServer* api_server_ = 0;

        std::vector<ButtonComponentWrapper*> button_components_ = {};
        std::vector<esphome::Component*> components_ = {};
        esphome::switch_::Switch* backlight_ = 0;
        esphome::rtttl::Rtttl* rtttl_ = 0;

        std::vector<DashboardPage*> page_objs_ = {};
        std::vector<DashboardButton*> button_objs_ = {};
        PageDef pages_[16] {};
        
        lv_obj_t* page_ = 0;
        lv_theme_t* theme__;

        lv_obj_t* buttons_ = 0;
        lv_obj_t* dashboard_btn_ = 0;
        lv_obj_t* connect_line_ = 0;

        lv_obj_t* more_page_ = 0;
        lv_obj_t* more_page_close_btn_ = 0;

        MoreInfoPage* more_info_page_ = 0;
        
        int width_ = 0;
        int height_ = 0;

        bool vertical_ = false;

        int page_no_ = 0;
        uint16_t dashboard_timeout_ = 0;

        esphome::lvgl::FontEngine* normal_font_ = 0;
        esphome::lvgl::FontEngine* large_font_ = 0;

        void show_page(int index);

        void send_event_(std::string type, std::function<void(esphome::api::HomeassistantServiceResponse*)> &&fn);
        void send_event(int page, int item, std::string type);
        void send_more_page_event(bool visible);

        lv_obj_t* create_more_page(lv_obj_t* root);
        lv_obj_t* create_buttons(lv_obj_t* root);

        bool has_buttons() { return this->button_components_.size() > 0; }
        void show_buttons(bool visible);
        bool buttons_visible();
        bool more_page_visible();
        bool turn_backlight();

        void clear_buttons();
        void clear_pages();
        void clear();

    public:

        static lv_obj_t* create_root_btn(lv_obj_t* root, std::string icon);

        void set_mdi_fonts(esphome::font::Font* small_font, esphome::font::Font* large_font);
        void set_fonts(esphome::font::Font* normal_font, esphome::font::Font* large_font) {
            this->normal_font_ = new esphome::lvgl::FontEngine(normal_font);
            if (large_font != 0) {
                this->large_font_ = new esphome::lvgl::FontEngine(large_font);
            }
        }
        void init(lv_obj_t* obj, bool init);
        void set_lvgl(esphome::lvgl::LvglComponent *root) { this->root_ = root; }
        void set_api_server(esphome::api::APIServer* api_server) { this->api_server_ = api_server; }
        void set_config(int width, int height) {
            this->width_ = width;
            this->height_ = height;
        }
        void set_vertical(bool vertical);
        void set_backlight(esphome::switch_::Switch* backlight) { this->backlight_ = backlight; }
        void set_rtttl(esphome::rtttl::Rtttl* rtttl) { this->rtttl_ = rtttl; }
        void set_dashboard_reset_timeout(uint16_t timeout) { this->dashboard_timeout_ = timeout; }
        void add_component(esphome::Component* component) { this->components_.push_back(component); }
        void setup() override;
        void update() override;

        void add_button_component(std::string type, esphome::EntityBase* component);

        void on_item_event(int page, int item, int event) override;
        void on_data_request(int page, int item) override;
        bool on_button(int index, lv_event_code_t event) override;
        void on_back_button(int page) override;

        float get_setup_priority() const override { return esphome::setup_priority::PROCESSOR; }

        void for_each_page(std::function<void(int, DashboardPage*)> &&fn, int page);
        void for_each_item(std::function<void(int, DashboardPage*, int, DashboardItem*)> &&fn, int page, int item);

        void set_buttons();
        void add_page(PageDef* page, int index);
        void set_pages(PageDef* pages, int size);

        void on_event(lv_event_t* event);
        void on_tap_event(lv_event_code_t code, lv_event_t* event);

        void show_more_page(JsonObject data);
        void hide_more_page();

        void update_connection_state();

        void service_set_pages(std::vector<std::string> pages, int page);
        void service_add_page(std::string page, bool reset);
        void service_set_value(int page, int item, std::string value);
        void service_set_data(int page, int item, int32_t* data, int size, int offset, int total_size);
        void service_show_page(int page);
        void service_set_button(int index, std::string json_value);
        void service_show_more(std::string json_value);
        void service_hide_more();
        void service_set_data_more(int32_t* data, int size, int offset, int total_size);
        void service_play_rtttl(std::string song);
        void service_set_theme(std::string json_value);
};

}
}
