"""
小智双足机器人 - 完整机械结构设计 (含固定方式)
运行方式: FreeCAD → Macro → Macros → Run
"""
import FreeCAD as App
import Part

SPHERE_DIAMETER = 100.0
SPLIT_Z = -5.0
WALL_THICKNESS = 3.0

SHELL_COLOR = 0.95, 0.95, 0.95, 0.85
LEG_COLOR = 0.9, 0.9, 0.9
SERVO_COLOR = 0.15, 0.15, 0.15
PCB_COLOR = 0.05, 0.25, 0.05
BATTERY_COLOR = 0.2, 0.2, 0.2
OLED_COLOR = 0.05, 0.05, 0.05
METAL_COLOR = 0.7, 0.7, 0.7
CLIP_COLOR = 0.85, 0.85, 0.85

doc = App.newDocument("Xiaozhi_Mechanical")

sphere_r = SPHERE_DIAMETER / 2.0

def make_box(name, x, y, z, l, w, h, color=None):
    obj = doc.addObject("Part::Box", name)
    obj.Length = l
    obj.Width = w
    obj.Height = h
    obj.Placement = App.Placement(App.Vector(x, y, z), App.Rotation(0, 0, 0))
    if color:
        obj.ViewObject.ShapeColor = color
    return obj

def make_cyl(name, x, y, z, r, h, rot=None, color=None):
    obj = doc.addObject("Part::Cylinder", name)
    obj.Radius = r
    obj.Height = h
    if rot:
        obj.Placement = App.Placement(App.Vector(x, y, z), rot)
    else:
        obj.Placement = App.Placement(App.Vector(x, y, z), App.Rotation(0, 0, 0))
    if color:
        obj.ViewObject.ShapeColor = color
    return obj

# ========== 1. 上半球 ==========
top_sphere = doc.addObject("Part::Sphere", "Top_Sphere")
top_sphere.Radius = sphere_r
top_sphere.Placement = App.Placement(App.Vector(0, 0, SPLIT_Z), App.Rotation(0, 0, 0))
top_sphere.ViewObject.ShapeColor = SHELL_COLOR

# OLED孔
oled_cut = make_box("OLED_Cut", -15, 28, SPLIT_Z - 15, 30, 8, 30)
top_shell = doc.addObject("Part::Cut", "Top_Shell")
top_shell.Base = top_sphere
top_shell.Tool = oled_cut
top_shell.ViewObject.ShapeColor = SHELL_COLOR

# 超声孔
ultrasonic_hole_top = make_cyl("Ultrasonic_Hole_Top", 0, 38, -35, 9, 12,
    App.Rotation(90, 0, 0))

# ========== 2. 下半球 ==========
bottom_sphere = doc.addObject("Part::Sphere", "Bottom_Sphere")
bottom_sphere.Radius = sphere_r
bottom_sphere.Placement = App.Placement(App.Vector(0, 0, SPLIT_Z), App.Rotation(0, 180, 0))
bottom_sphere.ViewObject.ShapeColor = SHELL_COLOR

# 舵机固定螺丝孔(下半球) - 8个M2孔，2个舵机各4个(2个舵机本体+2个臂)
# 左舵机固定孔位置 (舵机固定面朝向球心)
servo_mount_x_l = -32
servo_mount_y_l = -10
servo_mount_z_l = -40

# 右舵机固定孔位置
servo_mount_x_r = 19
servo_mount_y_r = -10
servo_mount_z_r = -40

# 充电孔
charging_hole = make_cyl("Charging_Hole", 0, 0, -50, 5, 12)

# 腿穿线孔 - 直径足够舵机线通过
leg_hole_l = make_cyl("LegHole_L", -32, 0, -45, 6, 12, App.Rotation(90, 0, 0))
leg_hole_r = make_cyl("LegHole_R", 19, 0, -45, 6, 12, App.Rotation(90, 0, 0))

# 切割下半球 - 顺序叠加切割
b1 = doc.addObject("Part::Cut", "B1")
b1.Base = bottom_sphere
b1.Tool = leg_hole_l

b2 = doc.addObject("Part::Cut", "B2")
b2.Base = b1
b2.Tool = leg_hole_r

b3 = doc.addObject("Part::Cut", "B3")
b3.Base = b2
b3.Tool = charging_hole

bottom_shell = doc.addObject("Part::Cut", "Bottom_Shell")
bottom_shell.Base = b3
bottom_shell.Tool = ultrasonic_hole_top
bottom_shell.ViewObject.ShapeColor = SHELL_COLOR

# ========== 3. 舵机固定螺丝(M2沉孔) ==========
# MG90固定孔：2个M2，间距约28mm
# 左舵机固定孔（穿透下半球）
m2_l_1 = make_cyl("M2_LH1", servo_mount_x_l, servo_mount_y_l - 5, servo_mount_z_l, 0.85, 12)
m2_l_2 = make_cyl("M2_LH2", servo_mount_x_l, servo_mount_y_l + 5, servo_mount_z_l, 0.85, 12)

# 右舵机固定孔
m2_r_1 = make_cyl("M2_RH1", servo_mount_x_r, servo_mount_y_r - 5, servo_mount_z_r, 0.85, 12)
m2_r_2 = make_cyl("M2_RH2", servo_mount_x_r, servo_mount_y_r + 5, servo_mount_z_r, 0.85, 12)

# ========== 4. MG90舵机本体 ==========
# 尺寸: 23 x 12.2 x 29mm
servo_l = make_box("MG90_Left", servo_mount_x_l - 11.5, servo_mount_y_l - 6, servo_mount_z_l + 3, 23, 12.2, 29, SERVO_COLOR)
servo_r = make_box("MG90_Right", servo_mount_x_r - 11.5, servo_mount_y_r - 6, servo_mount_z_r + 3, 23, 12.2, 29, SERVO_COLOR)

# 舵机轴(突出)
servo_shaft_l = make_cyl("Shaft_L", servo_mount_x_l, servo_mount_y_l, servo_mount_z_l, 2, 5, App.Rotation(0, 0, 0), METAL_COLOR)
servo_shaft_r = make_cyl("Shaft_R", servo_mount_x_r, servo_mount_y_r, servo_mount_z_r, 2, 5, App.Rotation(0, 0, 0), METAL_COLOR)

# ========== 5. 舵机臂(十字形) ==========
# 舵机臂 4个分支，每个长约10mm
# 左舵机臂
horn_arm_l1 = make_box("Horn_L_1", servo_mount_x_l - 1.5, servo_mount_y_l + 8, servo_mount_z_l, 3, 8, 2, LEG_COLOR)
horn_arm_l2 = make_box("Horn_L_2", servo_mount_x_l - 8, servo_mount_y_l - 1.5, servo_mount_z_l, 8, 3, 2, LEG_COLOR)

# 右舵机臂
horn_arm_r1 = make_box("Horn_R_1", servo_mount_x_r - 1.5, servo_mount_y_r + 8, servo_mount_z_r, 3, 8, 2, LEG_COLOR)
horn_arm_r2 = make_box("Horn_R_2", servo_mount_x_r - 8, servo_mount_y_r - 1.5, servo_mount_z_r, 8, 3, 2, LEG_COLOR)

# ========== 6. DevKitC-1 + 卡扣固定 ==========
# DevKitC-1 PCB本体 (水平放置)
devkit_l = 57.0
devkit_w = 31.0
devkit_h = 13.0

devkit = make_box("DevKitC1", -devkit_l/2, -devkit_w/2, SPLIT_Z - devkit_h - 2, devkit_l, devkit_w, 1.6, PCB_COLOR)

# 卡扣固定 - 4个角各一个卡扣
# 卡扣结构: 底座(粘在球壳内壁) + 卡槽(夹PCB边)
# 卡槽宽度: 1.6mm + 0.2mm间隙 = 1.8mm
clip_base_size = 6
clip_base_height = 3
clip_slot_width = 1.8
clip_slot_depth = 2.5

clip_positions = [
    (-devkit_l/2 - 1, -devkit_w/2 - 1),
    (devkit_l/2 + 1 - clip_base_size, -devkit_w/2 - 1),
    (-devkit_l/2 - 1, devkit_w/2 + 1 - clip_base_size),
    (devkit_l/2 + 1 - clip_base_size, devkit_w/2 + 1 - clip_base_size),
]

for i, (cx, cy) in enumerate(clip_positions):
    # 卡扣底座
    base = make_box(f"Clip_Base_{i+1}", cx, cy, SPLIT_Z - devkit_h - 2 - clip_base_height,
        clip_base_size, clip_base_size, clip_base_height, CLIP_COLOR)
    # 卡扣凸起(夹PCB)
    bump = make_box(f"Clip_Bump_{i+1}", cx + clip_base_size/2 - clip_slot_width/2,
        cy + clip_base_size/2 - clip_slot_width/2,
        SPLIT_Z - devkit_h - 2,
        clip_slot_width, clip_slot_width, clip_slot_depth, CLIP_COLOR)

# ========== 7. PCA9685 + 卡扣 ==========
pca9685 = make_box("PCA9685", 12, -devkit_w/2 - 21, SPLIT_Z - 8, 25, 19, 2, PCB_COLOR)

# PCA9685卡扣 - 4个角
pca_clip_positions = [
    (12 - 1, -devkit_w/2 - 21 - 1),
    (12 + 25 + 1 - clip_base_size, -devkit_w/2 - 21 - 1),
    (12 - 1, -devkit_w/2 - 21 + 19 + 1 - clip_base_size),
    (12 + 25 + 1 - clip_base_size, -devkit_w/2 - 21 + 19 + 1 - clip_base_size),
]

for i, (cx, cy) in enumerate(pca_clip_positions):
    base = make_box(f"PCA_Clip_Base_{i+1}", cx, cy, SPLIT_Z - 8 - clip_base_height,
        clip_base_size, clip_base_size, clip_base_height, CLIP_COLOR)
    bump = make_box(f"PCA_Clip_Bump_{i+1}", cx + clip_base_size/2 - clip_slot_width/2,
        cy + clip_base_size/2 - clip_slot_width/2,
        SPLIT_Z - 8,
        clip_slot_width, clip_slot_width, clip_slot_depth, CLIP_COLOR)

# ========== 8. INMP441 + MAX98357A ==========
inmp441 = make_box("INMP441", -devkit_l/2 - 14, -devkit_w/2 - 14, SPLIT_Z - 8, 12, 12, 1.5, (0.05, 0.25, 0.05))
max98357a = make_box("MAX98357A", -devkit_l/2 - 14, devkit_w/2 - 5, SPLIT_Z - 8, 15, 13, 2, (0.05, 0.25, 0.05))

# ========== 9. OLED ==========
oled = make_box("OLED", -13.5, 28 - 5, SPLIT_Z - 5, 27, 27, 3, OLED_COLOR)
oled_pcb = make_box("OLED_PCB", -10, 28 + 14, SPLIT_Z - 5, 20, 8, 2, PCB_COLOR)

# OLED固定卡扣 - 2个
for i, (cx, cy) in enumerate([(-13.5, 28 - 5), (-13.5 + 27 - 6, 28 - 5)]):
    base = make_box(f"OLED_Clip_{i+1}", cx, cy, SPLIT_Z - 5 - clip_base_height,
        6, 6, clip_base_height, CLIP_COLOR)
    bump = make_box(f"OLED_Clip_Bump_{i+1}", cx + 3 - clip_slot_width/2,
        cy + 3 - clip_slot_width/2,
        SPLIT_Z - 5,
        clip_slot_width, clip_slot_width, clip_slot_depth, CLIP_COLOR)

# ========== 10. HC-SR04 ============
hcsr04 = make_box("HC_SR04", -10, 38 - 7, SPLIT_Z - 5, 20, 15, 2, OLED_COLOR)
hcsr04_tr1 = make_cyl("US_TR1", -7, 38, SPLIT_Z - 5 + 3, 4, 6, App.Rotation(90, 0, 0), METAL_COLOR)
hcsr04_tr2 = make_cyl("US_TR2", 7, 38, SPLIT_Z - 5 + 3, 4, 6, App.Rotation(90, 0, 0), METAL_COLOR)

# ========== 11. 电池固定 - 电池仓 ==========
# 电池仓底部 + 两边卡扣
battery_chamber_base = make_box("Battery_Base", -25, -15, SPLIT_Z - 30, 50, 30, 1.5, CLIP_COLOR)
battery = make_box("LiPo_Battery", -25, -15, SPLIT_Z - 28, 50, 30, 15, BATTERY_COLOR)

# 电池卡扣 - 两侧各一个
for cy_off in [-15, 15 - clip_base_size]:
    base = make_box(f"Battery_Clip_{cy_off}", -25 + 10, cy_off, SPLIT_Z - 28 - 2 + 15,
        30, clip_base_size, 2, CLIP_COLOR)

# ========== 12. 腿结构 ==========
thigh_len = 50
foot_len = 35
foot_w = 30
foot_h = 4

# 左腿
thigh_l = make_cyl("Thigh_L", servo_mount_x_l, 0, servo_mount_z_l - thigh_len/2 - 5, 8, thigh_len,
    App.Rotation(90, 0, 0), LEG_COLOR)
knee_l = make_cyl("Knee_L", servo_mount_x_l, 0, servo_mount_z_l - thigh_len - 7.5 - 5, 10, 15,
    App.Rotation(90, 0, 0), LEG_COLOR)
foot_l = make_box("Foot_L", servo_mount_x_l - foot_len/2, 0, servo_mount_z_l - thigh_len - 15 - 5 - foot_h/2,
    foot_len, foot_w, foot_h, (0.2, 0.2, 0.2))

# 右腿
thigh_r = make_cyl("Thigh_R", servo_mount_x_r, 0, servo_mount_z_r - thigh_len/2 - 5, 8, thigh_len,
    App.Rotation(90, 0, 0), LEG_COLOR)
knee_r = make_cyl("Knee_R", servo_mount_x_r, 0, servo_mount_z_r - thigh_len - 7.5 - 5, 10, 15,
    App.Rotation(90, 0, 0), LEG_COLOR)
foot_r = make_box("Foot_R", servo_mount_x_r - foot_len/2, 0, servo_mount_z_r - thigh_len - 15 - 5 - foot_h/2,
    foot_len, foot_w, foot_h, (0.2, 0.2, 0.2))

# ========== 13. 大腿与舵机臂的连接件 ==========
# 连杆(舵机臂 → 大腿)
link_l = make_box("Link_L", servo_mount_x_l - 2, 0, servo_mount_z_l + 5, 4, 30, 2, METAL_COLOR)
link_r = make_box("Link_R", servo_mount_x_r - 2, 0, servo_mount_z_r + 5, 4, 30, 2, METAL_COLOR)

# ========== 14. 顶部USB-C和按钮（DevKitC-1自身） ==========
usbc = make_box("USB_C_Conn", -4, devkit_w/2 - 2, SPLIT_Z - 6, 8, 10, 3, (0.1, 0.1, 0.1))
button_boot = make_cyl("Boot_Btn", devkit_l/2 - 5, -devkit_w/2 + 3, SPLIT_Z - devkit_h - 2 + 1.6 + 1.5,
    1.5, 1.5, App.Rotation(0, 0, 0), METAL_COLOR)
button_reset = make_cyl("Reset_Btn", devkit_l/2 - 5, devkit_w/2 - 3, SPLIT_Z - devkit_h - 2 + 1.6 + 1.5,
    1, 1.5, App.Rotation(0, 0, 0), METAL_COLOR)

doc.recompute()

print("=" * 60)
print("完整机械结构设计")
print("=" * 60)
print(f"球体直径: {SPHERE_DIAMETER}mm")
print("=" * 60)
print("固定方式:")
print("  舵机: M2螺丝穿过球壳内壁 (2孔×2舵机 = 4孔)")
print("  DevKitC-1: 4个卡扣 (3D打印)")
print("  PCA9685: 4个卡扣 (3D打印)")
print("  OLED: 2个卡扣 (3D打印)")
print("  电池: 两侧卡扣 + 底座")
print("=" * 60)
print("所有零件:")
for obj in doc.Objects:
    if hasattr(obj, 'Shape'):
        print(f"  {obj.Name}")
print("=" * 60)