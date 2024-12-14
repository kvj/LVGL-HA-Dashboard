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

/*
switches:
  - pin: 0
  - button: true
pages:
  - rows: 4
    cols: 8
    tiles:
      - icon: "mdi:xxx"
        name: "Light"
        status: "100%"
        entity_id: xxx
        cols: 2
        rows: 2
        layout:
 */

namespace esphome {
namespace lvgl_dashboard {

#ifndef LVD_BG_COLOR
    #define LVD_BG_COLOR lv_palette_darken(LV_PALETTE_GREY, 4)
#endif
#ifndef LVD_TEXT_COLOR
    #define LVD_TEXT_COLOR lv_color_white()
#endif
#ifndef LVD_TEXT_ON_COLOR
    #define LVD_TEXT_ON_COLOR lv_color_black()
#endif
#ifndef LVD_PADDING
    #define LVD_PADDING 8
#endif
#ifndef LVD_BORDER_RADIUS
    #define LVD_BORDER_RADIUS 7
#endif
#ifndef LVD_PRESS_TRANSLATE_Y
    #define LVD_PRESS_TRANSLATE_Y 3
#endif
#ifndef LVD_PANEL_BG_COLOR
    #define LVD_PANEL_BG_COLOR lv_palette_darken(LV_PALETTE_GREY, 3)
#endif
#ifndef LVD_BTN_BG_COLOR
    #define LVD_BTN_BG_COLOR lv_palette_darken(LV_PALETTE_GREY, 2)
#endif
#ifndef LVD_BTN_PRESSED_COLOR
    #define LVD_BTN_PRESSED_COLOR lv_palette_darken(LV_PALETTE_GREY, 1)
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
#ifndef LVD_SWITCH_ON_LINE_COLOR
    #define LVD_SWITCH_ON_LINE_COLOR lv_palette_main(LV_PALETTE_ORANGE)
#endif


static esphome::lvgl::FontEngine* small_mdi_font = 0;
static esphome::lvgl::FontEngine* large_mdi_font = 0;

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
    ItemDef items[32];
    int items_size;
} PageDef;

class LvglItemEventListener {
    public:
        virtual void on_item_event(int page, int item, int event) = 0;
        virtual void on_data_request(int page, int item) = 0;
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
        void set_icon(lv_obj_t* obj, JsonObject icon_data);

        void destroy();
};

static lv_style_t item_style_normal_;
static lv_style_t item_style_pressed_;
class DashboardItem : public MdiFontCapable {

    protected:
        lv_obj_t* root_ = 0;
        ItemDef* def_ = 0;

        ItemEventListenerDef listener_{.item = 0, .page = 0, .listener = 0};

        lv_coord_t row_dsc_[4] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        lv_coord_t col_dsc_[4] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

        void set_bg_color(lv_obj_t* obj, JsonObject data);
        void set_text_color(lv_obj_t* obj, JsonObject data);

        void request_data();

    public:
        void set_definition(ItemDef* def) { this->def_ = def; }
        static void init(lv_obj_t* obj);
        lv_obj_t* get_lv_obj() { return this->root_; }

        virtual void setup(lv_obj_t* root);
        virtual void destroy();

        virtual void set_value(JsonObject data) {}
        virtual void set_data(int32_t* data, int size, int offset, int total_size) {}

        void loop();
        void on_tap_event(lv_event_code_t code, lv_event_t* event);

        void set_listener(int page, int item, LvglItemEventListener* listener);

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

class ImageItem : public DashboardItem {
    protected:
        uint8_t* data_ = 0;
        uint32_t data_size_ = 0;
        lv_img_dsc_t image_{};
    public:
        void setup(lv_obj_t* root) override;
        void set_value(JsonObject data) override;
        void set_data(int32_t* data, int size, int offset, int total_size) override;
        void destroy() override;

        uint8_t* create_data(uint32_t size);
        void draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t color);
        void show();
};

static lv_style_t btn_style_normal_;
static lv_style_t btn_style_pressed_;
static lv_style_t btn_line_style_normal_;
static lv_style_t btn_line_style_checked_;
static lv_style_t btn_line_style_pressed_;
class DashboardButton : public MdiFontCapable {
    protected:
        lv_obj_t* root_ = 0;
        lv_obj_t* line_ = 0;
        esphome::switch_::Switch* switch_ = 0;

        void update_state(bool state);

        DashboardButtonListenerDef listener_{.listener = 0};

        void set_side_icon(lv_obj_t* obj, JsonObject data);

    public:
        static void init(lv_obj_t* obj);
        void setup(lv_obj_t* root, esphome::switch_::Switch* switch_);
        void destroy();
        lv_obj_t* get_lv_obj() { return this->root_; }

        void on_event(lv_event_t* event);
        void on_tap_event(lv_event_code_t code, lv_event_t* event);
        void set_listener(int index, DashboardButtonListener* listener) {
            this->listener_.index = index;
            this->listener_.listener = listener;
        }
        void set_value(JsonObject data);
};

static lv_style_t page_style_;
class DashboardPage {
    protected:
        PageDef* def_ = 0;
        lv_obj_t* root_ = 0;

        std::vector<DashboardItem*> items_ = {};

        lv_coord_t row_dsc_[5] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        lv_coord_t col_dsc_[9] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), 
                                 LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    public:
        DashboardPage(PageDef* def) { this->def_ = def; }
        static void init(lv_obj_t* obj);
        void setup(lv_obj_t* parent, int page, LvglItemEventListener *listener);
        void destroy();
        lv_obj_t* get_lv_obj() { return this->root_; }

        void for_each_item(std::function<void(int, DashboardItem*)> &&fn, int item);
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
class MoreInfoPage {
    protected:
        std::function<void(std::string, std::string, int)> change_listener_ = 0;
        std::function<void(std::string, std::string)> data_request_listener_ = 0;
        std::vector<std::string> ids_ {};
        std::string entity_id_;

        uint8_t* data_ = 0;
        uint32_t data_size_ = 0;
        lv_img_dsc_t image_{};
        lv_obj_t *image_cmp_ = 0;

    public:
        static void init(lv_obj_t* obj);

        void setup(lv_obj_t* parent, JsonObject data);
        void destroy();

        void set_change_listener(std::function<void(std::string, std::string, int)> &&listener) {
            this->change_listener_ = listener;
        }

        void set_data_request_listener(std::function<void(std::string, std::string)> &&listener) {
            this->data_request_listener_ = listener;
        }

        void on_event(lv_event_t* event);
        void on_tap_event(lv_event_code_t code, lv_event_t* event);

        void set_data(int32_t* data, int size, int offset, int total_size);
};

static lv_style_t root_page_style_;
static lv_style_t sub_page_style_;
static lv_style_t top_style_;
static lv_style_t top_style_collapsed_;
static lv_style_t root_btn_style_normal_;
static lv_style_t root_btn_style_pressed_;
class LvglDashboard : virtual public LvglItemEventListener, public DashboardButtonListener, public esphome::Component  {
    private:

        lv_coord_t btns_row_dsc[2] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        lv_coord_t btns_col_dsc[5] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

        esphome::lvgl::LvglComponent* root_ = 0;
        esphome::api::APIServer* api_server_ = 0;

        std::vector<esphome::switch_::Switch*> switches_ = {};
        esphome::switch_::Switch* backlight_ = 0;
        esphome::rtttl::Rtttl* rtttl_ = 0;

        std::vector<DashboardPage*> page_objs_ = {};
        std::vector<DashboardButton*> button_objs_ = {};
        PageDef pages_[16] {};
        
        lv_theme_t* theme_;
        lv_obj_t* buttons_ = 0;
        lv_obj_t* dashboard_btn_ = 0;
        lv_obj_t* page_ = 0;
        lv_obj_t* sub_page_ = 0;
        lv_obj_t* sub_page_close_btn_ = 0;
        lv_obj_t* more_page_ = 0;
        lv_obj_t* more_page_close_btn_ = 0;
        MoreInfoPage* more_info_page_ = 0;
        
        int width_ = 0;
        int height_ = 0;

        int page_no_ = 0;
        uint16_t dashboard_timeout_ = 0;

        esphome::lvgl::FontEngine* normal_font_ = 0;

        void render_page(int index);
        void hide_page(int index);

        void show_page(int index);

        void send_event_(std::string type, std::function<void(esphome::api::HomeassistantServiceResponse*)> &&fn);
        void send_event(int page, int item, std::string type);
        void send_more_page_event(bool visible);

        lv_obj_t* create_root_btn(lv_obj_t* root, std::string icon);
        lv_obj_t* create_sub_page(lv_obj_t* root);
        lv_obj_t* create_more_page(lv_obj_t* root);
        lv_obj_t* create_root_page(lv_obj_t* root);
        lv_obj_t* create_buttons(lv_obj_t* root);

        bool has_buttons() { return this->switches_.size() > 0; }
        void show_buttons(bool visible);
        bool buttons_visible();
        bool more_page_visible();
        bool turn_backlight();

    public:

        void set_mdi_fonts(esphome::font::Font* small_font, esphome::font::Font* large_font);
        void set_font(esphome::font::Font* font) { this->normal_font_ = new esphome::lvgl::FontEngine(font); }
        void init(lv_obj_t* obj);
        void set_lvgl(esphome::lvgl::LvglComponent *root) { this->root_ = root; }
        void set_api_server(esphome::api::APIServer* api_server) { this->api_server_ = api_server; }
        void set_config(int width, int height) {
            this->width_ = width;
            this->height_ = height;
        }
        void set_backlight(esphome::switch_::Switch* backlight) { this->backlight_ = backlight; }
        void set_rtttl(esphome::rtttl::Rtttl* rtttl) { this->rtttl_ = rtttl; }
        void set_dashboard_reset_timeout(uint16_t timeout) { this->dashboard_timeout_ = timeout; }
        void setup() override;
        void loop() override;

        void add_switch(esphome::switch_::Switch* switch_) { this->switches_.push_back(switch_); }

        void on_item_event(int page, int item, int event) override;
        void on_data_request(int page, int item) override;
        bool on_button(int index, lv_event_code_t event) override;

        float get_setup_priority() const override { return esphome::setup_priority::PROCESSOR; }

        void for_each_page(std::function<void(int, DashboardPage*)> &&fn, int page);
        void for_each_item(std::function<void(int, DashboardPage*, int, DashboardItem*)> &&fn, int page, int item);

        void set_buttons();
        void set_pages(PageDef* pages, int size);

        void on_event(lv_event_t* event);
        void on_tap_event(lv_event_code_t code, lv_event_t* event);

        void show_more_page(JsonObject data);
        void hide_more_page();

        void service_set_pages(std::vector<std::string> pages, int page);
        void service_set_value(int page, int item, std::string value);
        void service_set_data(int page, int item, int32_t* data, int size, int offset, int total_size);
        void service_show_page(int page);
        void service_set_button(int index, std::string json_value);
        void service_show_more(std::string json_value);
        void service_hide_more();
        void service_set_data_more(int32_t* data, int size, int offset, int total_size);
        void service_play_rtttl(std::string song);
};

}
}
