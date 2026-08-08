#pragma once

#include <string>
#include "lvgl.h"
#include "app_types.h"

// ============================================================
// BASE COMPONENT
//
// Thin RAII wrapper over an LVGL object. Deleting the component
// deletes the widget tree rooted at m_root, so children created
// with m_root as their parent do not need explicit cleanup.
// ============================================================
class BaseComponent
{
protected:
    lv_obj_t *m_root;
    UIContext *m_ctx;

public:
    BaseComponent(UIContext *ctx) : m_root(nullptr), m_ctx(ctx) {}
    virtual ~BaseComponent();

    // Components own their LVGL objects, so copying them would lead
    // to a double lv_obj_delete.
    BaseComponent(const BaseComponent &) = delete;
    BaseComponent &operator=(const BaseComponent &) = delete;

    lv_obj_t *getRoot() const { return m_root; }
    void setVisible(bool visible);
};

class Label : public BaseComponent
{
public:
    Label(lv_obj_t *parent, UIContext *ctx, const char *text = "", lv_font_t *font = nullptr);

    void setText(const char *text);
    void setAlign(lv_text_align_t align);
    void setTextColor(lv_color_t color);
};

class QRCodePrimitive : public BaseComponent
{
private:
    // Kept so the code can be re-encoded when the widget is resized:
    // lv_qrcode_set_size() discards the existing image.
    std::string m_current_data;
    lv_coord_t m_size;

public:
    QRCodePrimitive(lv_obj_t *parent, UIContext *ctx, lv_coord_t size = 100,
                    lv_color_t dark = lv_color_black(), lv_color_t light = lv_color_white());

    void setSize(lv_coord_t size);
    void setData(const char *data);
};

// ============================================================
// STATUS BAR
//
// Three elements: an owner-defined badge on the left, the battery
// readout on the right, and the rotate button between them.
//
// The rotate button used to live in a separate ControlMenu strip at
// the bottom of the screen. Folding it in here reclaimed roughly
// 70 px of vertical space - about 13% of the panel height - which
// matters most in landscape, where the link list is cramped.
//
// The button carries two actions:
//   short tap  - rotate the screen 90 degrees
//   long press - force a full panel refresh
//
// The long press is deliberately undiscoverable. A full refresh is
// a maintenance action for when particle drift becomes visible
// before the automatic counter fires, and hiding it behind a
// deliberate gesture keeps it from being triggered by accident.
// ============================================================
class StatusBar : public BaseComponent
{
private:
    lv_obj_t *m_badge;
    Label *m_badge_label;
    Label *m_lbl_battery;
    lv_obj_t *m_btn_rotate;
    Label *m_lbl_rotate;

    static void rotate_btn_cb(lv_event_t *e);

public:
    StatusBar(lv_obj_t *parent, UIContext *ctx);
    ~StatusBar();

    void setBattery(const char *battery_str);
};