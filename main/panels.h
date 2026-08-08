#pragma once

#include "lvgl.h"
#include "app_types.h"
#include "widgets.h"
#include "socials.h"

// ============================================================
// PROFILE PANEL - avatar and caption
// ============================================================
class ProfilePanel : public BaseComponent
{
private:
    lv_obj_t *m_avatar;      // frame container: border only, no clipping
    lv_obj_t *m_avatar_clip; // inner container: clips the image
    lv_obj_t *m_img_widget;
    Label *m_name_label;

public:
    ProfilePanel(lv_obj_t *parent, UIContext *ctx);
    ~ProfilePanel();

    void updateSize(bool is_portrait);
};

// ============================================================
// QR PANEL - code plus the name of whatever it points at
// ============================================================
class QrPanel : public BaseComponent
{
private:
    QRCodePrimitive *m_qr;
    Label *m_lbl_platform;

public:
    QrPanel(lv_obj_t *parent, UIContext *ctx);
    ~QrPanel();

    void showLink(const char *platform_label, const char *url);
    void setSize(lv_coord_t qr_size);
};

// ============================================================
// SOCIAL PANEL - the selectable link list
// ============================================================
class SocialPanel : public BaseComponent
{
private:
    struct RowWidgets
    {
        lv_obj_t *row;
        lv_obj_t *chip;
        Label *chip_label;
        Label *name_label;
    };

    // Fixed-size array: this is why SOCIAL_LINKS_COUNT has to be a
    // compile-time constant, and why the link table lives in a header.
    RowWidgets m_rows[SOCIAL_LINKS_COUNT];

    int m_selected_index;
    void (*m_on_select)(void *, int);
    void *m_on_select_user_data;

    void styleRow(int index, bool selected);
    static void row_click_cb(lv_event_t *e);

public:
    SocialPanel(lv_obj_t *parent, UIContext *ctx);
    ~SocialPanel();

    void updateLayout(bool is_portrait);
    void setOnSelect(void (*cb)(void *, int), void *user_data);
    void selectIndex(int index);
    void clearSelection();
};

// ============================================================
// CARD SCENE - the three panels and their layout
//
// The bottom ControlMenu strip that used to sit below these panels
// is gone: its rotate button moved into the status bar and its
// sleep button was removed entirely. Sleep now happens on an
// inactivity timeout, so a manual button did nothing the firmware
// would not do by itself thirty seconds later.
//
// Layout flips between a row (landscape) and a column (portrait).
// ============================================================
class CardScene : public BaseComponent
{
private:
    ProfilePanel *m_profile;
    QrPanel *m_qr_panel;
    SocialPanel *m_social;

    static void on_social_select(void *user_data, int index);

public:
    CardScene(lv_obj_t *parent, UIContext *ctx);
    ~CardScene();

    void loadCustomLink(const char *url);
    void updateLayout(bool is_portrait);
    void selectSocialByIndex(int idx);
};