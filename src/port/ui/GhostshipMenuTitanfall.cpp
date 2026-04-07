#include "GhostshipMenu.h"

namespace GhostshipGui {

extern std::shared_ptr<GhostshipMenu> mGhostshipMenu;

using namespace UIWidgets;

#define CVAR_TF(var) "gTitanfall." var

void GhostshipMenu::AddMenuTitanfall() {
    AddMenuEntry("Titanfall", CVAR_SETTING("Menu.TitanfallSidebarSection"));

    /* ── Movement ─────────────────────────────────────────── */
    WidgetPath path = { "Titanfall", "Movement", SECTION_COLUMN_1 };
    AddSidebarEntry("Titanfall", "Movement", 2);

    AddWidget(path, "Ground", WIDGET_SEPARATOR_TEXT);

    AddWidget(path, "Ground Max Speed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Ground.MaxSpeed"))
        .Options(FloatSliderOptions().Min(10.0f).Max(80.0f).DefaultValue(38.0f).Format("%.0f"));

    AddWidget(path, "Ground Acceleration", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Ground.Accel"))
        .Options(FloatSliderOptions().Min(1.0f).Max(30.0f).DefaultValue(10.0f).Format("%.1f"));

    AddWidget(path, "Ground Friction", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Ground.Friction"))
        .Options(FloatSliderOptions().Min(0.5f).Max(15.0f).DefaultValue(6.0f).Format("%.1f"));

    AddWidget(path, "Air", WIDGET_SEPARATOR_TEXT);

    AddWidget(path, "Air Acceleration", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Air.Accel"))
        .Options(FloatSliderOptions().Min(1.0f).Max(30.0f).DefaultValue(12.0f).Format("%.1f"));

    AddWidget(path, "Air Cap", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Air.Cap"))
        .Options(FloatSliderOptions().Min(1.0f).Max(15.0f).DefaultValue(4.0f).Format("%.1f"));

    AddWidget(path, "Gravity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Air.Gravity"))
        .Options(FloatSliderOptions().Min(30.0f).Max(200.0f).DefaultValue(105.0f).Format("%.0f"));

    AddWidget(path, "Jumping", WIDGET_SEPARATOR_TEXT);

    AddWidget(path, "Jump Velocity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Jump.Vel"))
        .Options(FloatSliderOptions().Min(20.0f).Max(100.0f).DefaultValue(60.0f).Format("%.0f"));

    AddWidget(path, "Double Jump Velocity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Jump.DoubleVel"))
        .Options(FloatSliderOptions().Min(20.0f).Max(100.0f).DefaultValue(50.0f).Format("%.0f"));

    /* ── Column 2: Slide ──────────────────────────────────── */
    path.column = SECTION_COLUMN_2;

    AddWidget(path, "Slide", WIDGET_SEPARATOR_TEXT);

    AddWidget(path, "Slide Min Speed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Slide.MinSpeed"))
        .Options(FloatSliderOptions().Min(5.0f).Max(40.0f).DefaultValue(20.0f).Format("%.0f"));

    AddWidget(path, "Slide Boost", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Slide.Boost"))
        .Options(FloatSliderOptions().Min(0.0f).Max(30.0f).DefaultValue(8.0f).Format("%.1f"));

    AddWidget(path, "Slide Friction", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Slide.Friction"))
        .Options(FloatSliderOptions().Min(0.0f).Max(5.0f).DefaultValue(0.3f).Format("%.2f"));

    AddWidget(path, "Slide Jump Boost", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Slide.JumpBoost"))
        .Options(FloatSliderOptions().Min(1.0f).Max(1.5f).DefaultValue(1.15f).Format("%.2f"));

    /* ── Wallrun ──────────────────────────────────────────── */
    path = { "Titanfall", "Wallrun", SECTION_COLUMN_1 };
    AddSidebarEntry("Titanfall", "Wallrun", 2);

    AddWidget(path, "Entry", WIDGET_SEPARATOR_TEXT);

    AddWidget(path, "Min Speed to Attach", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WR.MinSpeed"))
        .Options(FloatSliderOptions().Min(1.0f).Max(40.0f).DefaultValue(12.0f).Format("%.0f"));

    AddWidget(path, "Min Height to Attach", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WR.MinHeight"))
        .Options(FloatSliderOptions().Min(5.0f).Max(80.0f).DefaultValue(20.0f).Format("%.0f"));

    AddWidget(path, "Approach Angle Min (dot)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WR.ApproachMin"))
        .Options(FloatSliderOptions().Min(0.0f).Max(0.5f).DefaultValue(0.10f).Format("%.2f")
            .Tooltip("Lower = more parallel approaches allowed"));

    AddWidget(path, "Approach Angle Max (dot)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WR.ApproachMax"))
        .Options(FloatSliderOptions().Min(0.5f).Max(1.0f).DefaultValue(0.92f).Format("%.2f")
            .Tooltip("Higher = more head-on approaches allowed"));

    AddWidget(path, "Duration", WIDGET_SEPARATOR_TEXT);

    AddWidget(path, "Max Duration (frames)", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_TF("WR.MaxFrames"))
        .Options(IntSliderOptions().Min(15).Max(120).DefaultValue(52).ShowButtons(true)
            .Tooltip("At 30fps: 30 = 1s, 52 = 1.75s"));

    AddWidget(path, "Gravity Scale", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WR.GravityScale"))
        .Options(FloatSliderOptions().Min(0.0f).Max(1.0f).DefaultValue(0.15f).Format("%.2f")
            .Tooltip("Gravity multiplier while wallrunning"));

    AddWidget(path, "Entry Upkick", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WR.EntryUpkick"))
        .Options(FloatSliderOptions().Min(0.0f).Max(30.0f).DefaultValue(12.0f).Format("%.1f")
            .Tooltip("Upward boost when attaching to wall"));

    /* Column 2: Wall-kick */
    path.column = SECTION_COLUMN_2;

    AddWidget(path, "Wall-Kick (from wallrun)", WIDGET_SEPARATOR_TEXT);

    AddWidget(path, "Kick Normal Force", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WR.KickNormal"))
        .Options(FloatSliderOptions().Min(10.0f).Max(80.0f).DefaultValue(35.0f).Format("%.0f")
            .Tooltip("Outward impulse when kicking off wall"));

    AddWidget(path, "Kick Up Force", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WR.KickUp"))
        .Options(FloatSliderOptions().Min(10.0f).Max(80.0f).DefaultValue(35.0f).Format("%.0f")
            .Tooltip("Upward impulse when kicking off wall"));

    AddWidget(path, "Kick Speed Preserve", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WR.KickPreserve"))
        .Options(FloatSliderOptions().Min(0.0f).Max(1.0f).DefaultValue(0.70f).Format("%.0f %%").IsPercentage()
            .Tooltip("Fraction of along-wall speed kept on kick"));

    AddWidget(path, "Chain Speed Bonus", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WR.KickBonus"))
        .Options(FloatSliderOptions().Min(1.0f).Max(1.3f).DefaultValue(1.08f).Format("%.2f")
            .Tooltip("Speed multiplier per wall-kick chain"));

    AddWidget(path, "Wall-Kick (from air)", WIDGET_SEPARATOR_TEXT);

    AddWidget(path, "Air Wall-Kick Normal", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WK.Normal"))
        .Options(FloatSliderOptions().Min(10.0f).Max(60.0f).DefaultValue(30.0f).Format("%.0f")
            .Tooltip("Outward impulse for air wall-kick"));

    AddWidget(path, "Air Wall-Kick Up", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("WK.Up"))
        .Options(FloatSliderOptions().Min(10.0f).Max(80.0f).DefaultValue(45.0f).Format("%.0f")
            .Tooltip("Upward impulse for air wall-kick"));

    /* ── Camera ───────────────────────────────────────────── */
    path = { "Titanfall", "Camera", SECTION_COLUMN_1 };
    AddSidebarEntry("Titanfall", "Camera", 1);

    AddWidget(path, "Sensitivity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Cam.Sensitivity"))
        .Options(FloatSliderOptions().Min(0.5f).Max(5.0f).DefaultValue(1.8f).Format("%.1f"));

    AddWidget(path, "Distance (0 = FPS)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Cam.Distance"))
        .Options(FloatSliderOptions().Min(0.0f).Max(1500.0f).DefaultValue(700.0f).Format("%.0f"));

    AddWidget(path, "Height", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Cam.Height"))
        .Options(FloatSliderOptions().Min(0.0f).Max(300.0f).DefaultValue(120.0f).Format("%.0f"));

    AddWidget(path, "FOV", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Cam.FOV"))
        .Options(FloatSliderOptions().Min(45.0f).Max(120.0f).DefaultValue(75.0f).Format("%.0f"));

    AddWidget(path, "Wallrun Camera Roll", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_TF("Cam.WallrunRoll"))
        .Options(FloatSliderOptions().Min(0.0f).Max(25.0f).DefaultValue(5.0f).Format("%.1f"));
}

} // namespace GhostshipGui
