
class LVGLDashboardEditor extends HTMLElement {
    set hass(hass) {
        if (!this.hass_) {
            console.log(`LVGLDashboardEditor::hass set:`, hass);
            this.hass_ = hass;
        }
    }

    setConfig(config) {
        console.log(`LVGLDashboardEditor::setConfig`, config, this.hass_);
        this.deviceSelector = document.createElement("ha-selector-device");
        this.deviceSelector.hass = this.hass_;
        this.deviceSelector.selector = {"device": {}};
        this.deviceSelector.setAttribute("label", "Device:");
        this.appendChild(this.deviceSelector);
    }
};

customElements.define("lvgl-dashboard-editor", LVGLDashboardEditor);

const createIconEl = (root, name, size) => {
    const icon = root.appendChild(document.createElement("ha-icon"));
    icon.setAttribute("icon", `mdi:${name}`);
    icon.setAttribute("style", `--mdc-icon-size: ${size}px;`);
    return icon;
}

class DashboardItem extends EventTarget {
    constructor(theme) {
        super();
        this.root = document.createElement("button");
        this.defaultClasses = "item radius";
        this.root.className = this.defaultClasses;

        this.root.addEventListener("click", (event) => {
            console.log(`DashboardItem::addEventListener`, event);
            this.dispatchEvent(new CustomEvent("tap", {detail: {long: false}}));
        });
    }

    destroy() {}

    setTextColor(obj, el) {
        el.style.color = "";
        const color = obj["col"];
        const mode = obj["ctype"];
        if (!color) return el.classList.add("text-color");
        if (mode == "text") {
            if (color == "on") return el.classList.add("text-btn-on-color");
            if (color && color.length == 7) return el.style.color = color;
            return el.classList.add("text-color");
        }
        if (color) return el.classList.add("text-on-color");
    }

    setBackgroundColor(obj, el, def_color) {
        el.style.backgroundColor = "";
        const color = obj["col"];
        const mode = obj["ctype"];
        if (!color) return el.classList.add(def_color || "btn-bg-color");
        if (mode == "text") return el.classList.add(def_color || "btn-bg-color");
        if (color == "on") return el.classList.add("btn-on-color");
        if (color && color.length == 7) return el.style.backgroundColor = color;
        return el.classList.add(def_color || "btn-bg-color");
    }
    
    element() {
        return this.root;
    }

    setValue(obj, theme) {
    }

    createIconEl(root, obj) {
        return createIconEl(root, obj["name"], obj["size"]);
    }

    createSpanWrapperEl(root) {
        const spanDiv = root.appendChild(document.createElement("div"));
        spanDiv.className = "flex-label";
        return spanDiv;
    }
};

class ButtonItem extends DashboardItem {
    constructor(theme) {
        super(theme);
    }

    setValue(obj, theme) {
        this.root.innerHTML = ``;
        this.root.className = `${this.defaultClasses} button-layout`;
        this.setBackgroundColor(obj, this.root);

        let el = this.createSpanWrapperEl(this.root);
        el.style.gridColumn = "1";
        el.style.gridRow = "2";

        const span = el.appendChild(document.createElement("div"));
        span.className = "normal-text single-line";
        span.textContent = obj["name"] || "";
        this.setTextColor(obj, span);

        el = this.createSpanWrapperEl(this.root);
        const icon = el.appendChild(document.createElement("ha-icon"));
        icon.setAttribute("icon", `mdi:${obj["icon"]["name"]}`);
        icon.setAttribute("style", `--mdc-icon-size: ${obj["icon"]["size"]}px;`);
        this.setTextColor(obj, icon);
        el.style.gridColumn = "1";
        el.style.gridRow = "1";
    }

};
class SensorItem extends DashboardItem {
    constructor(theme) {
        super(theme);
    }
    setValue(obj, theme) {
        this.root.innerHTML = ``;
        this.root.className = `${this.defaultClasses} sensor-layout`;
        this.setBackgroundColor(obj, this.root, "panel-bg-color");

        let el = this.createSpanWrapperEl(this.root);
        const icon = this.createIconEl(el, obj["icon"]);
        this.setTextColor(obj, icon);
        el.style.gridColumn = "1";
        el.style.gridRow = "1";
        el.style.alignItems = "end";

        el = this.createSpanWrapperEl(this.root);
        el.style.gridColumn = "2 / span 2";
        el.style.gridRow = "1";
        el.style.alignItems = "end";

        const span = el.appendChild(document.createElement("div"));
        span.className = "normal-text single-line";
        span.textContent = obj["name"] || "";
        this.setTextColor(obj, span);

        el = this.createSpanWrapperEl(this.root);
        el.style.justifyContent = "end";
        el.style.alignItems = "end";
        el.style.gridColumn = "1 / span 2";
        el.style.gridRow = "2";

        const value = el.appendChild(document.createElement("div"));
        value.className = "large-text single-line";
        value.textContent = obj["value"] || "";
        this.setTextColor(obj, value);

        if (obj["unit"]) {
            el = this.createSpanWrapperEl(this.root);
            el.style.gridColumn = "3";
            el.style.gridRow = "2";
            el.style.alignItems = "end";
    
            const unit = el.appendChild(document.createElement("div"));
            unit.className = "small-text single-line";
            unit.textContent = obj["unit"];
            this.setTextColor(obj, unit);
        } else {
            el.style.gridColumn = "1 / span 3";
        }
    }
};
class PictureItem extends DashboardItem {
    constructor(theme) {
        super(theme);
    }
    setValue(obj, theme) {
        this.root.innerHTML = ``;
        this.root.className = `${this.defaultClasses} picture-layout`;
        this.setBackgroundColor(obj, this.root);
        if (obj["image"]) {
            const img = this.root.appendChild(document.createElement("img"));
            img.className = "picture-img";
            img.setAttribute("src", obj["image"]["uri"]);
        }
    }
};

class LayoutItem extends DashboardItem {
    constructor(theme) {
        super(theme);
    }
    setValue(obj, theme) {
        this.root.innerHTML = ``;
        this.root.className = `${this.defaultClasses} layout-layout`;
        this.setBackgroundColor(obj, this.root);
        const rows = obj["rows"].map(item => `${item}fr`).join(" ");
        const cols = obj["cols"].map(item => `${item}fr`).join(" ")
        this.root.style.gridTemplate = `${rows} / ${cols}`;
        let col = 0;
        let row = 0;
        for (let i = 0; i < obj["items"].length; i++) {
            const item = obj["items"][i];
            if (item["_h"] == true) continue;
            const w = item["w"] > 1? item["w"]: 1;
            const h = item["h"] > 1? item["h"]: 1;
            const x = item["x"] >= 0? item["x"]: col;
            const y = item["y"] >= 0? item["y"]: row;

            const el = this.createSpanWrapperEl(this.root);
            if (item["icon"]) {
                const icon = this.createIconEl(el, item["icon"]);
                this.setTextColor(item, icon);
            } else {
                const label = el.appendChild(document.createElement("div"));
                label.className = "normal-text single-line";
                label.textContent = item["label"] || "";
                this.setTextColor(item, label);
            }
            el.style.gridColumn = w > 1? `${x + 1} / span ${w}`: `${x + 1}`;
            el.style.gridRow = h > 1? `${y + 1} / span ${h}`: `${y + 1}`;

            col = x + w;
            row = y;
        }
    }
};

const itemMap = {
    "button": ButtonItem,
    "sensor": SensorItem,
    "picture": PictureItem,
    "layout": LayoutItem,
};

class DashboardPage extends EventTarget {

    constructor(obj, index, theme) {
        super();
        this.index = index;
        this.root = document.createElement("div");
        this.dashboardEl_ = this.createDashboard(obj.rows, obj.cols, obj.items, theme);
        if (index > 0) {
            const body = this.root.appendChild(document.createElement("div"));
            body.className = "sub-page";
            const backBtn = body.appendChild(document.createElement("button"));
            backBtn.className = "item padding radius btn-bg-color";
            createIconEl(backBtn, "arrow-left", 50);
            body.appendChild(this.dashboardEl_);
            backBtn.addEventListener("click", () => {
                this.dispatchEvent(new CustomEvent("back", {detail: {}}));
            });
        } else {
            this.root.appendChild(this.dashboardEl_);
        }
    }

    listenToEvents(index, obj) {
        obj.addEventListener("tap", (event) => {
            console.log(`DashboardPage::listenToEvents`, event);
            this.dispatchEvent(new CustomEvent("tap", {detail: {item: index}}));
        });
    }

    createDashboard(rows, cols, items, theme) {
        const root = document.createElement("div");
        root.style.display = "grid";
        root.style.gridTemplate = `repeat(${rows}, 1fr) / repeat(${cols}, 1fr)`;
        root.style.gap = `${theme.padding}px`;

        let col = 0;
        let row = 0;
        this.items = [];
        for (let i = 0; i < items.length; i++) {
            const item = items[i];
            const cols_ = item["cols"] || 1;
            const rows_ = item["rows"] || 1;
            col = item["col"] >= 0? item["col"]: col;
            row = item["row"] >= 0? item["row"]: row;
            const cls = itemMap[item["layout"]];
            if (cls) {
                const item_ = new cls(theme);
                this.listenToEvents(i, item_);
                this.items.push(item_);
                const btn = item_.element();
                btn.style.gridColumn = cols_ > 1? `${col + 1} / span ${cols_}`: `${col + 1}`;
                btn.style.gridRow = rows_ > 1? `${row + 1} / span ${rows_}`: `${row + 1}`;
                root.appendChild(btn);
            } else {
                console.log(`DashboardPage::constructor: unsupported layout:`, item);
                this.items.push(undefined);
            }
            col += cols_;
            if (col >= cols) {
                col = 0;
                row++;
            }
        };
        return root;
    }

    setValue(item, obj, theme) {
        // console.log(`DashboardPage::setValue`, item, obj);
        if (item < this.items.length) {
            const item_ = this.items[item];
            if (item_) {
                if (obj["_h"] == true) {
                    item_.element().style.visibility = "hidden";
                } else {
                    item_.setValue(obj, theme);
                    item_.element().style.visibility = "visible";
                }
            }
        }
    }

    element() {
        return this.root;
    }

    dashboard() {
        return this.dashboardEl_;
    }

    destroy() {
        this.items.forEach((item) => {
            item.destroy();
        });
        this.items = [];
    }
};

class LVGLDashboard extends HTMLElement {

    constructor() {
        super();
        this.pages = [];
        this.page_no = 0;
        this.eventSubscription = undefined;
        this.wasConnected = false;
        console.log(`LVGLDashboard::constructor`);
    }

    setTheme(obj) {
        console.log(`setTheme:`, obj);
        this.theme = {
            text_color: "#FAFAFA",
            text_on_color: "#212121",
            panel_bg_color: "#424242",
            btn_bg_color: "#616161",
            btn_pressed_color: "#757575",
            btn_on_color: "#FFEB3B",
            padding: 8,
            radius: 7,
            font_family: `"Ubuntu Condensed", "Nimbus Sans Narrow", "Roboto Condensed", "Arial Narrow", sans-serif`,
            normal_font: 14,
            small_font: 12,
            large_font: 28,
            ...obj,
        };
        this.updateStyle();
    }

    listenToEvents(index, page) {
        page.addEventListener("tap", (event) => {
            console.log(`LVGLDashboard::listenToEvents`, event);
            this.sendEvent("click", {page: index, item: event.detail.item});
        });
        page.addEventListener("back", () => {
            this.showPage(0);
        });
    }

    setPages(objects) {
        console.log(`setPages`, objects);
        this.pages.forEach((page) => {
            page.destroy();
        });
        this.pages = objects.map((item, index) => {
            return new DashboardPage(item, index, this.theme);
        });
        this.root.innerHTML = ``;
        
        this.pages.forEach((page, index) => {
            this.listenToEvents(index, page);
            const dashboard = page.dashboard();
            dashboard.className = "main-container";
            this.root.appendChild(page.element());
        });
        this.showPage(0);
    }

    setValue(page, item, obj) {
        console.log(`setValue`, page, item, obj);
        if (page < this.pages.length) this.pages[page].setValue(item, obj, this.theme);
    }

    showPage(page) {
        console.log(`showPage`, page);
        this.pages.forEach((item, index) => {
            item.element().style.display = index == page? "contents": "none";
        });
        this.sendEvent("page", {page: page});
    }

    showMoreDialog(data, visible) {
        console.log(`showMoreDialog`, data, visible);
        const evt = new CustomEvent("hass-more-info", {
            bubbles: true,
            composed: true,
            cancelable: false,
            detail: { entityId: visible? data["id"]: undefined },
        });
        document.querySelector("home-assistant").dispatchEvent(evt);
        if (visible) {
            this.sendEvent("more", {"visible": "1"});
        }
    }

    handleEvent(name, data) {
        if (this.isConnected) {
            this.wasConnected = true;
        } else {
            if (this.wasConnected) {
                if (this.eventSubscription) {
                    console.log(`handleEvent stopping event listener`, this.eventSubscription);
                    this.eventSubscription();
                    this.eventSubscription = undefined;    
                }
                return;
            }
        }
        switch (name) {
            case "set_theme":
                this.setTheme(JSON.parse(data.json_value));
                break;
            case "set_pages":
                this.setPages(data.jsons.map((item) => JSON.parse(item)));
                break;
            case "set_value":
                this.setValue(data.page, data.item, JSON.parse(data.json_value));
                break;
            case "show_page":
                this.showPage(data.page);
                break;
            case "show_more":
                this.showMoreDialog(JSON.parse(data.json_value), true);
                break;
            case "hide_more":
                this.showMoreDialog(undefined, false);
                break;
        }
    }

    updateStyle() {
        const mw = this.config_["width"] || 1024;
        const mh = this.config_["height"] || 768;
        const zoom = this.config_["zoom"] || 1;

        this.styleTag.textContent = `
        .root {
            zoom: ${zoom};
        }
        .flex-center {
            width: 100%;
            height: 100%;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        .flex-label {
            height: 100%;
            display: inline-grid;
            align-items: center;
            justify-content: center;
        }
        .sub-page {
            display: flex;
            justify-content: start;
            align-items: start;
            flex-direction: row;
            gap: ${this.theme.padding}px;
        }
        .contents {
            display: contents;
        }        
        .main-container {
            width: 100%;
            max-width: ${mw}px;
            aspect-ratio: ${mw}/${mh};
        }
        .panel-bg-color {
            background-color: ${this.theme.panel_bg_color};
        }
        .btn-bg-color {
            background-color: ${this.theme.btn_bg_color};
        }
        .btn-on-color {
            background-color: ${this.theme.btn_on_color};
        }
        .text-btn-on-color {
            color: ${this.theme.btn_on_color};
        }
        .text-color {
            color: ${this.theme.text_color};
        }
        .text-on-color {
            color: ${this.theme.text_on_color};
        }
        .radius {
            border-radius: ${this.theme.radius}px;
            border-width: 0px;
        }
        .padding {
            padding: ${this.theme.padding}px;
        }
        .item {
            margin: 0px;
        }
        .item:active {
            background-color: ${this.theme.btn_pressed_color};
        }
        .button-layout {
            display: grid;
            grid-template: 1fr auto / 1fr;
        }
        .sensor-layout {
            display: grid;
            grid-template: 30px 1fr / 30px 1fr auto;
        }
        .layout-layout {
            display: grid;
        }
        .picture-layout {
            display: flex;
            justify-content: center;
            align-items: center;
        }
        .normal-text {
            font-family: ${this.theme.font_family};
            font-size: ${this.theme.normal_font}px;
        }
        .small-text {
            font-family: ${this.theme.font_family};
            font-size: ${this.theme.small_font}px;
        }
        .large-text {
            font-family: ${this.theme.font_family};
            font-size: ${this.theme.large_font}px;
            font-weight: bold;
        }
        .single-line {
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        `;
    }

    createRoot() {
        this.shadow = this.attachShadow({ mode: "open" });

        this.styleTag = document.createElement("style");
        this.shadow.appendChild(this.styleTag);

        this.body = document.createElement("div");
        this.shadow.appendChild(this.body);

        this.body.className = "flex-center root";

        this.style.width = "100%";
        this.style.height = "100%";

        this.innerHTML = ``;

        this.root = document.createElement("div");
        this.root.className = "contents";
        this.body.appendChild(this.root);
        this.setTheme({}); // Default theme
    }

    set hass(hass) {
        if (!this.hass_) {
            this.hass_ = hass;
            if (!this.validate()) return;

            this.hass_.connection.subscribeEvents((event) => {
                // console.log(`Event:`, event);
                if (event.data.config_entry_id == this.config_entry_id) {
                    this.handleEvent(event.data.name, event.data.data);
                }
            }, "lvgl_dashboard_service_call").then((result) => {
                console.log(`Subscription:`, result);
                this.eventSubscription = result;
            });
            console.log(`LVGLDashboard::hass set:`);
            this.sendEvent("online", null);
        }
    }

    sendEvent(event, data) {
        this.hass_.connection.sendMessage({
            type: "lvgl_dashboard/event",
            event: event,
            config_entry_id: this.config_entry_id,
            data: data || {},
        });
    }

    validate() {
        if (!this.hass_ || !this.config_) return false;
        const device = this.hass_.devices[this.config_["device_id"]];
        console.log(`LVGLDashboard::validate`, device);
        if (!device) return false;
        this.config_entry_id = device.primary_config_entry;
        this.createRoot();

        document.querySelector("home-assistant").addEventListener("dialog-closed", (evt) => {
            if (this.isConnected) {
                console.log(`dialog-closed`, evt);
                this.sendEvent("more", {"visible": "0"});
            }
        });

        return true;
    }

    setConfig(config) {
        console.log(`LVGLDashboard::setConfig`, config);
        this.config_ = config;
    }

    // static getConfigElement() {
    //     return document.createElement("lvgl-dashboard-editor");
    // }

    // static getStubConfig() {
    //     return {"device_id": "change me"};
    // }
};

customElements.define("lvgl-dashboard", LVGLDashboard);
