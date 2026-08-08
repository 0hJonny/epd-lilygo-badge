#include "widgets.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "app_config.h"
#include "user_profile.h"
#include "ui_strings.h"

static const char *TAG = "WIDGETS";

// ============================================================
// BASE COMPONENT
// ============================================================
BaseComponent::~BaseComponent()
{
    if (m_root && lv_obj_is_valid(m_root))
        lv_obj_delete(m_root);
}

void BaseComponent::setVisible(bool visible)
{
    if (visible)
        lv_obj_remove_flag(m_root, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(m_root, LV_OBJ_FLAG_HIDDEN);
}

// ============================================================
// LABEL
// ============================================================
Label::Label(lv_obj_t *parent, UIContext *ctx, const char *text, lv_font_t *font) : BaseComponent(ctx)
{
    m_root = lv_label_create(parent);
    lv_label_set_text(m_root, text);
    if (font)
        lv_obj_set_style_text_font(m_root, font, 0);
}

void Label::setText(const char *text) { lv_label_set_text(m_root, text); }
void Label::setAlign(lv_text_align_t align) { lv_obj_set_style_text_align(m_root, align, 0); }
void Label::setTextColor(lv_color_t color) { lv_obj_set_style_text_color(m_root, color, 0); }

// ============================================================
// QR CODE
// ============================================================
QRCodePrimitive::QRCodePrimitive(lv_obj_t *parent, UIContext *ctx, lv_coord_t size,
                                 lv_color_t dark, lv_color_t light)
    : BaseComponent(ctx), m_size(size)
{
    m_root = lv_qrcode_create(parent);
    lv_qrcode_set_size(m_root, size);
    lv_qrcode_set_dark_color(m_root, dark);
    lv_qrcode_set_light_color(m_root, light);
}

void QRCodePrimitive::setSize(lv_coord_t size)
{
    if (m_size == size)
        return;
    m_size = size;
    lv_qrcode_set_size(m_root, size);

    // Resizing discards the encoded image, so it has to be
    // regenerated from the stored payload.
    if (!m_current_data.empty())
        lv_qrcode_update(m_root, m_current_data.c_str(), m_current_data.length());
}

void QRCodePrimitive::setData(const char *data)
{
    m_current_data = data;
    lv_qrcode_update(m_root, m_current_data.c_str(), m_current_data.length());
}

// ============================================================
// STATUS BAR
// ============================================================
void StatusBar::rotate_btn_cb(lv_event_t *e)
{
    StatusBar *self = (StatusBar *)lv_event_get_user_data(e);
    if (!self->m_ctx->app_queue)
        return;

    lv_event_code_t code = lv_event_get_code(e);

    UIEvent cmd;
    cmd.payload = nullptr;

    if (code == LV_EVENT_LONG_PRESSED)
    {
        cmd.command = CommandType::FULL_REFRESH;
    }
    else if (code == LV_EVENT_SHORT_CLICKED)
    {
        cmd.command = CommandType::CYCLE_ORIENTATION;
    }
    else
    {
        return;
    }

    // Zero timeout: this runs inside LVGL's event dispatch, and
    // blocking here would stall rendering. Dropping a press is
    // better than stalling the display, and the queue only fills up
    // if the display task is already wedged.
    if (xQueueSend(self->m_ctx->app_queue, &cmd, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "Command queue full, button press dropped");
    }
}

StatusBar::StatusBar(lv_obj_t *parent, UIContext *ctx) : BaseComponent(ctx)
{
    m_root = lv_obj_create(parent);
    lv_obj_remove_flag(m_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(m_root, LV_PCT(100));
    lv_obj_set_height(m_root, LV_SIZE_CONTENT);

    lv_obj_set_style_pad_all(m_root, 6, 0);

    lv_obj_set_style_border_width(m_root, 2, 0);
    lv_obj_set_style_border_side(m_root, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(m_root, lv_color_black(), 0);
    lv_obj_set_style_radius(m_root, 0, 0);

    lv_obj_set_layout(m_root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(m_root, LV_FLEX_FLOW_ROW);
    // Badge on the left, battery and button pushed to the right.
    lv_obj_set_flex_align(m_root, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // ------------------------------------------------------------
    // Owner badge. Same visual language as the social chips, so the
    // interface reads as one family rather than two.
    // ------------------------------------------------------------
    m_badge = lv_obj_create(m_root);
    lv_obj_remove_flag(m_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(m_badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(m_badge, 40, 40);
    lv_obj_set_style_radius(m_badge, 6, 0);
    lv_obj_set_style_border_width(m_badge, 0, 0);
    lv_obj_set_style_pad_all(m_badge, 0, 0);
    lv_obj_set_style_bg_color(m_badge, lv_color_black(), 0);

    m_badge_label = new Label(m_badge, ctx, UserProfile::STATUS_BADGE, ctx->font_24);
    m_badge_label->setTextColor(lv_color_white());
    lv_obj_center(m_badge_label->getRoot());

    // ------------------------------------------------------------
    // Battery readout. Starts as a placeholder: showing an invented
    // number before the first ADC sample would be a lie, and this
    // firmware previously shipped with a hard-coded "85%".
    // ------------------------------------------------------------
    m_lbl_battery = new Label(m_root, ctx, ui().battery_unknown, ctx->font_24);
    // Fixed width so the flex row does not reflow when the text
    // changes from a placeholder to a number, or from 100% to 99%.
    // Without it the rotate button shifts by a pixel or two on every
    // battery update and gets repainted along with it - a visible
    // flicker on e-ink for no reason.
    lv_obj_set_width(m_lbl_battery->getRoot(), STATUS_BATTERY_LABEL_WIDTH);
    m_lbl_battery->setAlign(LV_TEXT_ALIGN_RIGHT);

    // ------------------------------------------------------------
    // Rotate button.
    //
    // remove_style_all strips LVGL's default theme, which includes
    // press animations and colour transitions. Both are meaningless
    // on e-ink - the panel cannot render intermediate frames - and
    // each transition would trigger an extra redraw.
    // ------------------------------------------------------------
    m_btn_rotate = lv_button_create(m_root);
    lv_obj_remove_style_all(m_btn_rotate);
    // 48 px is the practical minimum for a finger-sized touch target,
    // and it sets the height of the whole status bar.
    lv_obj_set_size(m_btn_rotate, 56, 48);
    lv_obj_set_style_bg_color(m_btn_rotate, lv_color_white(), 0);
    lv_obj_set_style_border_width(m_btn_rotate, 2, 0);
    lv_obj_set_style_border_color(m_btn_rotate, lv_color_black(), 0);
    lv_obj_set_style_radius(m_btn_rotate, 8, 0);

    // SHORT_CLICKED rather than CLICKED: CLICKED also fires after a
    // long press, which would rotate the screen every time the user
    // asked for a refresh.
    lv_obj_add_event_cb(m_btn_rotate, rotate_btn_cb, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb(m_btn_rotate, rotate_btn_cb, LV_EVENT_LONG_PRESSED, this);

    // A symbol rather than a word: it needs no translation, costs no
    // Japanese glyphs, and fits where a text label would not.
    m_lbl_rotate = new Label(m_btn_rotate, ctx, LV_SYMBOL_LOOP, nullptr);
    m_lbl_rotate->setTextColor(lv_color_black());
    lv_obj_center(m_lbl_rotate->getRoot());
}

StatusBar::~StatusBar()
{
    delete m_badge_label;
    delete m_lbl_battery;
    delete m_lbl_rotate;
}

void StatusBar::setBattery(const char *battery_str) { m_lbl_battery->setText(battery_str); }