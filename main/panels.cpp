#include "panels.h"

#include <stdio.h>
#include <stdint.h>

#include "app_config.h"
#include "embedded_assets.h"
#include "user_profile.h"
#include "ui_strings.h"

// ============================================================
// SHARED AVATAR CONSTRUCTION
//
// The avatar is built from two nested containers rather than one.
//
// A single object cannot both draw a border and clip its content:
// clip_corner crops the border along with everything else, and the
// image on top covers whatever survives. Worse, enabling clip_corner
// before the widget has a size makes LVGL build its corner mask from
// a zero-sized rectangle, which recalculates on every frame.
//
// So: the outer container owns the border, the inner one owns the
// clipping, and sizes for both are set later by the caller.
//
// Used by both ProfilePanel and SleepPanel.
// ============================================================
static void build_avatar(lv_obj_t *parent, lv_obj_t **out_frame,
                         lv_obj_t **out_clip, lv_obj_t **out_img)
{
    lv_obj_t *frame = lv_obj_create(parent);
    lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(frame, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(frame, 0, 0);
    lv_obj_set_style_clip_corner(frame, true, 0);
    lv_obj_set_style_border_color(frame, lv_color_black(), 0);
    lv_obj_set_style_outline_color(frame, lv_color_black(), 0);

    lv_obj_t *clip = lv_obj_create(frame);
    lv_obj_remove_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(clip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(clip, 0, 0);
    lv_obj_set_style_border_width(clip, 0, 0);
    lv_obj_center(clip);

    switch (UserProfile::PHOTO_FRAME)
    {
    case UserProfile::AvatarFrame::NONE:
        lv_obj_set_style_border_width(frame, 0, 0);
        lv_obj_set_style_radius(frame, 16, 0);
        lv_obj_set_style_radius(clip, 16, 0);
        break;

    case UserProfile::AvatarFrame::THIN:
        lv_obj_set_style_border_width(frame, 2, 0);
        lv_obj_set_style_radius(frame, 16, 0);
        lv_obj_set_style_radius(clip, 14, 0);
        break;

    case UserProfile::AvatarFrame::THICK:
        lv_obj_set_style_border_width(frame, 6, 0);
        lv_obj_set_style_radius(frame, 16, 0);
        lv_obj_set_style_radius(clip, 10, 0);
        break;

    case UserProfile::AvatarFrame::SQUARE:
        lv_obj_set_style_border_width(frame, 3, 0);
        lv_obj_set_style_radius(frame, 0, 0);
        lv_obj_set_style_radius(clip, 0, 0);
        break;

    case UserProfile::AvatarFrame::DOUBLE:
        lv_obj_set_style_border_width(frame, 2, 0);
        lv_obj_set_style_radius(frame, 16, 0);
        lv_obj_set_style_radius(clip, 14, 0);
        lv_obj_set_style_outline_width(frame, 2, 0);
        // The gap is what makes this read as two frames rather than
        // one thick line. The outline draws outside the widget
        // bounds, so the flex parent needs room for it.
        lv_obj_set_style_outline_pad(frame, 4, 0);
        lv_obj_set_style_margin_all(frame, 8, 0);
        break;
    }

    lv_obj_t *img = lv_image_create(clip);
    lv_image_set_src(img, &avatar);
    lv_obj_center(img);

    *out_frame = frame;
    *out_clip = clip;
    *out_img = img;
}

// Resize an avatar built by build_avatar. The clipping container has
// to shrink by twice the border width, or the image covers the frame
// from the inside and the border disappears.
static void size_avatar(lv_obj_t *frame, lv_obj_t *clip, lv_obj_t *img,
                        int32_t size)
{
    lv_obj_set_size(frame, size, size);

    int32_t border = lv_obj_get_style_border_width(frame, LV_PART_MAIN);
    int32_t inner = size - border * 2;
    lv_obj_set_size(clip, inner, inner);

    // Enable clipping only now that a real size exists. Doing it at
    // construction makes LVGL compute the corner mask from a
    // zero-sized rectangle and recalculate it every frame.
    lv_obj_set_style_clip_corner(clip, true, 0);

    // Scale factor is fixed-point with 256 meaning 1.0. Deriving it
    // from the source width means any square avatar resolution works
    // without touching this code.
    int32_t scale = (inner * 256) / avatar.header.w;
    lv_image_set_scale(img, scale);
}

// ============================================================
// PROFILE PANEL
// ============================================================
ProfilePanel::ProfilePanel(lv_obj_t *parent, UIContext *ctx) : BaseComponent(ctx)
{
    m_root = lv_obj_create(parent);
    lv_obj_remove_flag(m_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(m_root, 0, 0);
    lv_obj_set_style_pad_all(m_root, 0, 0);

    lv_obj_set_layout(m_root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(m_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    build_avatar(m_root, &m_avatar, &m_avatar_clip, &m_img_widget);

    m_name_label = new Label(m_root, ctx, UserProfile::PHOTO_SIGN, ctx->font_24);
    lv_obj_set_style_margin_top(m_name_label->getRoot(), 15, 0);
}

ProfilePanel::~ProfilePanel()
{
    delete m_name_label;
}

void ProfilePanel::updateSize(bool is_portrait)
{
    size_avatar(m_avatar, m_avatar_clip, m_img_widget, is_portrait ? 200 : 240);
}

// ============================================================
// QR PANEL
// ============================================================
QrPanel::QrPanel(lv_obj_t *parent, UIContext *ctx) : BaseComponent(ctx)
{
    m_root = lv_obj_create(parent);
    lv_obj_remove_flag(m_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(m_root, 0, 0);
    lv_obj_set_style_pad_all(m_root, 0, 0);

    lv_obj_set_layout(m_root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(m_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_qr = new QRCodePrimitive(m_root, ctx, 160);
    m_lbl_platform = new Label(m_root, ctx, "", ctx->font_24);
    lv_obj_set_style_margin_top(m_lbl_platform->getRoot(), 10, 0);
}

QrPanel::~QrPanel()
{
    delete m_qr;
    delete m_lbl_platform;
}

void QrPanel::showLink(const char *platform_label, const char *url)
{
    m_qr->setData(url);

    char buf[80];
    snprintf(buf, sizeof(buf), ui().qr_caption_fmt, platform_label);
    m_lbl_platform->setText(buf);
}

void QrPanel::setSize(lv_coord_t qr_size) { m_qr->setSize(qr_size); }

// ============================================================
// SOCIAL PANEL
// ============================================================
void SocialPanel::styleRow(int index, bool selected)
{
    // Selection is shown by inverting the row. On a monochrome panel
    // that is the only contrast cue available - no accent colors, no
    // shadows, no animation.
    RowWidgets &r = m_rows[index];
    if (selected)
    {
        lv_obj_set_style_bg_color(r.row, lv_color_black(), 0);
        r.name_label->setTextColor(lv_color_white());
        lv_obj_set_style_bg_color(r.chip, lv_color_white(), 0);
        r.chip_label->setTextColor(lv_color_black());
    }
    else
    {
        lv_obj_set_style_bg_color(r.row, lv_color_white(), 0);
        r.name_label->setTextColor(lv_color_black());
        lv_obj_set_style_bg_color(r.chip, lv_color_black(), 0);
        r.chip_label->setTextColor(lv_color_white());
    }
}

void SocialPanel::row_click_cb(lv_event_t *e)
{
    SocialPanel *panel = (SocialPanel *)lv_event_get_user_data(e);
    lv_obj_t *clicked = (lv_obj_t *)lv_event_get_target(e);

    // The index is stashed in the row's user data at construction, so
    // this is O(1) and survives any reordering of the widget tree.
    int index = (int)(intptr_t)lv_obj_get_user_data(clicked);
    panel->selectIndex(index);
}

SocialPanel::SocialPanel(lv_obj_t *parent, UIContext *ctx)
    : BaseComponent(ctx), m_selected_index(-1),
      m_on_select(nullptr), m_on_select_user_data(nullptr)
{
    m_root = lv_obj_create(parent);
    lv_obj_remove_flag(m_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(m_root, 0, 0);
    lv_obj_set_style_pad_all(m_root, 0, 0);

    lv_obj_set_style_pad_row(m_root, SOCIAL_ROW_GAP, 0);
    lv_obj_set_style_pad_column(m_root, SOCIAL_ROW_GAP, 0);

    lv_obj_set_layout(m_root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(m_root, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(m_root, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    for (int i = 0; i < SOCIAL_LINKS_COUNT; i++)
    {
        RowWidgets &r = m_rows[i];
        r.row = lv_obj_create(m_root);
        lv_obj_remove_flag(r.row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_radius(r.row, 8, 0);
        lv_obj_set_style_border_width(r.row, 2, 0);
        lv_obj_set_style_border_color(r.row, lv_color_black(), 0);
        lv_obj_set_style_pad_all(r.row, 6, 0);

        lv_obj_set_layout(r.row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(r.row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r.row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_add_flag(r.row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(r.row, (void *)(intptr_t)i);
        lv_obj_add_event_cb(r.row, row_click_cb, LV_EVENT_CLICKED, this);

        r.chip = lv_obj_create(r.row);
        lv_obj_remove_flag(r.chip, LV_OBJ_FLAG_SCROLLABLE);
        // Base LVGL objects are clickable by default, so without this
        // the chip swallowed taps and never forwarded them: pressing
        // the monogram - a 36x36 target on the left of every row -
        // did nothing at all.
        lv_obj_remove_flag(r.chip, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(r.chip, 36, 36);
        lv_obj_set_style_radius(r.chip, 6, 0);
        lv_obj_set_style_border_width(r.chip, 0, 0);

        r.chip_label = new Label(r.chip, ctx, SOCIAL_LINKS[i].monogram, ctx->font_24);
        lv_obj_center(r.chip_label->getRoot());

        r.name_label = new Label(r.row, ctx, SOCIAL_LINKS[i].label, ctx->font_24);
        lv_obj_set_style_margin_left(r.name_label->getRoot(), 12, 0);

        styleRow(i, false);
    }
}

SocialPanel::~SocialPanel()
{
    for (int i = 0; i < SOCIAL_LINKS_COUNT; i++)
    {
        delete m_rows[i].chip_label;
        delete m_rows[i].name_label;
    }
}

void SocialPanel::updateLayout(bool is_portrait)
{
    // Portrait fits two columns of rows; landscape has the width for
    // one full-width column but far less height.
    for (int i = 0; i < SOCIAL_LINKS_COUNT; i++)
    {
        lv_obj_set_width(m_rows[i].row, is_portrait ? LV_PCT(47) : LV_PCT(95));
        lv_obj_set_height(m_rows[i].row,
                          is_portrait ? SOCIAL_ROW_HEIGHT_PORTRAIT : SOCIAL_ROW_HEIGHT_LANDSCAPE);
    }
}

void SocialPanel::setOnSelect(void (*cb)(void *, int), void *user_data)
{
    m_on_select = cb;
    m_on_select_user_data = user_data;
}

void SocialPanel::selectIndex(int index)
{
    // Re-selecting the current row is a deliberate no-op: repainting
    // it would cost a panel refresh for no visible change.
    if (index < 0 || index >= SOCIAL_LINKS_COUNT || index == m_selected_index)
        return;

    if (m_selected_index >= 0)
        styleRow(m_selected_index, false);

    m_selected_index = index;
    styleRow(m_selected_index, true);

    if (m_on_select)
        m_on_select(m_on_select_user_data, index);
}

void SocialPanel::clearSelection()
{
    if (m_selected_index >= 0)
    {
        styleRow(m_selected_index, false);
        m_selected_index = -1;
    }
}

// ============================================================
// CARD SCENE
// ============================================================
void CardScene::on_social_select(void *user_data, int index)
{
    CardScene *self = (CardScene *)user_data;

    // Persisted in RTC memory so the same link comes back after a
    // reboot or a wake from sleep.
    g_selected_social_index = index;

    self->m_qr_panel->showLink(SOCIAL_LINKS[index].label, SOCIAL_LINKS[index].url);
}

CardScene::CardScene(lv_obj_t *parent, UIContext *ctx) : BaseComponent(ctx)
{
    // A single container. This used to be a column wrapper holding a
    // content container plus a ControlMenu strip; with the strip gone
    // the wrapper had exactly one child, so the two were merged. One
    // less nesting level, and a few dozen fewer style allocations -
    // every lv_obj_set_style_* call allocates.
    m_root = lv_obj_create(parent);
    lv_obj_remove_flag(m_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(m_root, LV_PCT(100));
    lv_obj_set_flex_grow(m_root, 1);
    lv_obj_set_style_border_width(m_root, 0, 0);
    lv_obj_set_style_pad_all(m_root, 10, 0);
    lv_obj_set_layout(m_root, LV_LAYOUT_FLEX);

    m_profile = new ProfilePanel(m_root, ctx);
    m_qr_panel = new QrPanel(m_root, ctx);
    m_social = new SocialPanel(m_root, ctx);

    m_social->setOnSelect(on_social_select, this);
}

CardScene::~CardScene()
{
    delete m_profile;
    delete m_qr_panel;
    delete m_social;
}

void CardScene::loadCustomLink(const char *url)
{
    m_social->clearSelection();
    m_qr_panel->showLink(ui().custom_link, url);
}

void CardScene::updateLayout(bool is_portrait)
{
    lv_obj_set_flex_flow(m_root, is_portrait ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);

    if (is_portrait)
    {
        lv_obj_set_size(m_profile->getRoot(), LV_PCT(100), LV_PCT(33));
        lv_obj_set_size(m_qr_panel->getRoot(), LV_PCT(100), LV_PCT(33));
        lv_obj_set_size(m_social->getRoot(), LV_PCT(100), LV_PCT(34));
    }
    else
    {
        // Landscape gives the link list 40% and splits the rest
        // evenly: long labels need the extra width or they compress
        // into unreadable columns.
        lv_obj_set_size(m_profile->getRoot(), LV_PCT(30), LV_PCT(100));
        lv_obj_set_size(m_qr_panel->getRoot(), LV_PCT(30), LV_PCT(100));
        lv_obj_set_size(m_social->getRoot(), LV_PCT(40), LV_PCT(100));
    }

    m_profile->updateSize(is_portrait);
    m_qr_panel->setSize(is_portrait ? 180 : 220);
    m_social->updateLayout(is_portrait);
}

void CardScene::selectSocialByIndex(int idx) { m_social->selectIndex(idx); }

// ============================================================
// SLEEP PANEL
// ============================================================
SleepPanel::SleepPanel(lv_obj_t *parent, UIContext *ctx) : BaseComponent(ctx)
{
    m_root = lv_obj_create(parent);
    lv_obj_remove_flag(m_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(m_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(m_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(m_root, 0, 0);
    lv_obj_set_style_pad_all(m_root, 10, 0);
    lv_obj_set_style_bg_color(m_root, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(m_root, LV_OPA_COVER, 0);

    lv_obj_set_layout(m_root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(m_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Avatar and QR live in a nested container so the hint can sit
    // pinned at the bottom while these two stay centred together.
    m_content = lv_obj_create(m_root);
    lv_obj_remove_flag(m_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(m_content, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(m_content, LV_PCT(100));
    lv_obj_set_flex_grow(m_content, 1);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, 0, 0);
    lv_obj_set_layout(m_content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_align(m_content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    build_avatar(m_content, &m_avatar, &m_avatar_clip, &m_img_widget);

    m_qr = new QRCodePrimitive(m_content, ctx, 200);

    m_sign = new Label(m_root, ctx, UserProfile::SLEEP_SIGN, ctx->font_24);
    lv_obj_set_style_margin_top(m_sign->getRoot(), 12, 0);

    m_hint = new Label(m_root, ctx, ui().sleep_hint, ctx->font_24);
    lv_obj_set_style_margin_top(m_hint->getRoot(), 8, 0);

    // The QR is fixed for the lifetime of the panel: which link it
    // encodes is a build-time choice, not a runtime one.
    constexpr int idx = UserProfile::SLEEP_LINK_INDEX;
    static_assert(idx >= 0 && idx < SOCIAL_LINKS_COUNT,
                  "UserProfile::SLEEP_LINK_INDEX is out of range for SOCIAL_LINKS");
    m_qr->setData(SOCIAL_LINKS[idx].url);

    // Hidden until the badge is actually going to sleep.
    lv_obj_add_flag(m_root, LV_OBJ_FLAG_HIDDEN);
}

SleepPanel::~SleepPanel()
{
    delete m_qr;
    delete m_sign;
    delete m_hint;
}

void SleepPanel::updateLayout(bool is_portrait)
{
    // Portrait stacks avatar over QR; landscape places them side by
    // side, where vertical space is the scarce dimension.
    lv_obj_set_flex_flow(m_content, is_portrait ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);

    if (is_portrait)
    {
        lv_obj_set_style_pad_row(m_content, 30, 0);
        size_avatar(m_avatar, m_avatar_clip, m_img_widget, 180);
        m_qr->setSize(220);
    }
    else
    {
        lv_obj_set_style_pad_column(m_content, 50, 0);
        size_avatar(m_avatar, m_avatar_clip, m_img_widget, 200);
        m_qr->setSize(240);
    }
}