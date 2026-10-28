# OneButton case: Fusion 360 build script (v0.8 geometry, spec section numbers refer to OneButton_design_spec.md)
# Run in Fusion (Utilities > Scripts, or via the Fusion MCP "script" tool) in an EMPTY parametric design.
# Spec axes: Z up, +Y towards the antenna end, USB-C at -Y, BOOT on -X, RST on +X.
# Fusion is Y-up, so spec (x, y, z) -> Fusion (x, z, -y). All values below are spec mm.
# Creates bodies in the root component: REF_* (reference only, do not print), CASE_SHELL, BOTTOM_CAP, KEYCAP, SHROUD.
# Set NEW_DOCUMENT = True to build into a fresh scratch document instead of the active one.
import adsk.core, adsk.fusion, math

NEW_DOCUMENT = False
LOG_PATH = '/Users/brettsamuel/Documents/Fusion360/OneButton/onebutton_build.log'

def log(msg):
    try:
        with open(LOG_PATH, 'a') as fh:
            fh.write(msg + '\n')
    except Exception:
        pass
EXPORT_DIR = None   # e.g. '/Users/you/Documents/Fusion360/OneButton' to write the four STLs at the end

# ---------------- parameters ----------------
CASE_W, CASE_L, R_OUT = 30.0, 39.0, 5.0
Z_BOT, Z_TOP, SKIN_Z, WEB_Z = -17.1, 0.0, -3.0, -1.5
CAV_W, CAV_L, R_CAV = 26.0, 35.0, 1.5
SW_CUT, SW_RELIEF = 14.0, 16.0
PEG_XY = (10.2, 13.85)            # board mount holes (+/-x, y); a second pair of plain posts sits at y = -13.85
POST_D, POST_BOT = 4.0, -10.3
PEG_D, PEG_BOT = 1.8, -14.8   # v0.12: pegs go through the board (underside Z -11.8) and stand 3.0 proud, to be heat-staked
USB_W, USB_Z0, USB_Z1, R_USB = 9.8, -16.2, -10.7, 1.0   # v0.10: 5.5 tall, centred on the connector (Z -13.45), was Z -15.5..-11.4
BOOT_Y, BOOT_HALF, BOOT_Z0, BOOT_Z1, R_BOOT = -13.75, 3.25, -15.2, -11.6, 0.5
RST_Z, RST_D = -13.3, 2.0
WIN_Z0, WIN_Z1 = -15.2, -13.2
WIN_Y = [(5.5, 10.5), (-8.5, -3.5)]
SHROUD_PEG_XY, SHROUD_PEG_HOLE_D, SHROUD_PEG_HOLE_DEPTH = (11.0, 15.0), 3.2, 2.2
SHROUD_PEG_D, SHROUD_PEG_LEN = 3.0, 2.0
CAP_W, CAP_L, R_CAP, CAP_T = 25.6, 34.6, 1.3, 2.0
CAP_BOSS_TOP = -11.9
HOOK_TIP, HOOK_BASE_X, HOOK_ARM_X = 14.0, 11.4, 12.8
HOOK_Z0, HOOK_CATCH_Z, HOOK_Z1, HOOK_LEN = -15.1, -14.7, -13.7, 3.6
HOOKS_Y = [(6.2, 9.8), (-7.8, -4.2)]
SLOT_RECTS = [  # (x0, y0, x1, y1) on the +x side, mirrored for -x
    (10.2, 1.4, 11.4, 10.0), (10.2, 10.0, 12.9, 11.2),
    (10.2, -8.0, 11.4, 0.6), (10.2, -9.2, 12.9, -8.0)]
KEY_W, KEY_R, KEY_WALL = 24.3, 4.15, 1.2
KEY_PLATE_Z0, KEY_PLATE_T = 10.0, 3.0
KEY_SKIRT_Z0, KEY_SLEEVE_Z0, KEY_SLEEVE_D = 6.5, 6.6, 5.6
KEY_CROSS_L, KEY_CROSS_W, KEY_CROSS_TOP = 4.15, 1.40, 10.2
SH_BASE_T, SH_TOP_Z, SH_TOP_W, SH_TOP_R = 1.5, 8.5, 28.0, 6.0   # v0.11: top was 11.5
SH_BORE_W, SH_BORE_R = 24.6, 4.3
# switch (Kailh Box Navy datasheet)
SW_HOUSING_W, SW_HOUSING_TOP = 15.6, 6.2
SW_LOWER_W, SW_LOWER_BOT = 14.0, -5.0
STEM_BASE_TOP, STEM_CROSS_TOP = 6.4, 10.0

V = adsk.core.ValueInput
P3 = adsk.core.Point3D
F = adsk.fusion.FeatureOperations


def run(context):
    open(LOG_PATH, 'w').close()
    log('start')
    app = adsk.core.Application.get()
    if NEW_DOCUMENT:
        app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    des = adsk.fusion.Design.cast(app.activeProduct)
    log('designType %s' % des.designType)
    root = des.rootComponent
    BASE = {'XZ': root.xZConstructionPlane, 'XY': root.xYConstructionPlane, 'YZ': root.yZConstructionPlane}

    def to_world(kind, c, u, v):
        if kind == 'XZ': x, y, z = u, v, c
        elif kind == 'XY': x, y, z = u, c, v
        else: x, y, z = c, u, v
        return P3.create(x * 0.1, z * 0.1, -y * 0.1)

    def world_off(kind, c):
        return {'XZ': c, 'XY': -c, 'YZ': c}[kind]

    def axis_of(kind):
        return {'XZ': 1, 'XY': 2, 'YZ': 0}[kind]

    def new_sketch(kind, c, name):
        want = world_off(kind, c) * 0.1
        for sgn in (1, -1):
            inp = root.constructionPlanes.createInput()
            inp.setByOffset(BASE[kind], V.createByReal(sgn * abs(want)))
            pl = root.constructionPlanes.add(inp)
            if abs(pl.geometry.origin.asArray()[axis_of(kind)] - want) < 1e-6:
                break
            pl.deleteMe()
        sk = root.sketches.add(pl)
        sk.name = name
        return pl, sk

    class S:  # sketch + coordinate helper
        def __init__(s, kind, c, name):
            log('sketch ' + name)
            s.kind, s.c = kind, c
            s.pl, s.sk = new_sketch(kind, c, name)
        def q(s, u, v):
            return s.sk.modelToSketchSpace(to_world(s.kind, s.c, u, v))
        def rect(s, u0, v0, u1, v1, r=0.0):
            lines = s.sk.sketchCurves.sketchLines.addTwoPointRectangle(s.q(u0, v0), s.q(u1, v1))
            if r > 0:
                ls = [lines.item(i) for i in range(4)]
                def mid(l):
                    a, b = l.geometry.startPoint, l.geometry.endPoint
                    return P3.create((a.x + b.x) / 2, (a.y + b.y) / 2, 0)
                for i in range(4):
                    s.sk.sketchCurves.sketchArcs.addFillet(ls[i], mid(ls[i]), ls[(i + 1) % 4], mid(ls[(i + 1) % 4]), r * 0.1)
        def crect(s, w, h, r=0.0, cu=0.0, cv=0.0):
            s.rect(cu - w / 2, cv - h / 2, cu + w / 2, cv + h / 2, r)
        def circle(s, u, v, d):
            s.sk.sketchCurves.sketchCircles.addByCenterRadius(s.q(u, v), d / 2 * 0.1)
        def poly(s, pts):
            n = len(pts)
            for i in range(n):
                a, b = pts[i], pts[(i + 1) % n]
                s.sk.sketchCurves.sketchLines.addByTwoPoints(s.q(*a), s.q(*b))
        def all_profiles(s):
            pc = adsk.core.ObjectCollection.create()
            for i in range(s.sk.profiles.count): pc.add(s.sk.profiles.item(i))
            return pc
        def profile_with_loops(s, n):
            best = None
            for i in range(s.sk.profiles.count):
                p = s.sk.profiles.item(i)
                if p.profileLoops.count == n: best = p
            return best
        def largest(s):
            return max((s.sk.profiles.item(i) for i in range(s.sk.profiles.count)), key=lambda p: p.areaProperties().area)

    def ext(s, prof, spec_dir, dist, op, name, parts=None):
        log('ext ' + name)
        """Extrude profile along the spec axis normal to the sketch. spec_dir +1 = increasing spec coordinate of that axis."""
        wdir = {'XZ': (0, 1, 0), 'XY': (0, 0, -1), 'YZ': (1, 0, 0)}[s.kind]
        n = s.pl.geometry.normal.asArray()
        sign = spec_dir * (1 if sum(a * b for a, b in zip(n, wdir)) > 0 else -1)
        inp = root.features.extrudeFeatures.createInput(prof, op)
        inp.setDistanceExtent(False, V.createByReal(sign * dist * 0.1))
        if parts is not None: inp.participantBodies = parts
        f = root.features.extrudeFeatures.add(inp)
        f.name = name
        return f

    def new_body(f, name):
        b = f.bodies.item(0); b.name = name; return b

    NB, J, C = F.NewBodyFeatureOperation, F.JoinFeatureOperation, F.CutFeatureOperation
    UP, DN = 1, -1

    # ================= REF bodies (interference checks only, not printed) =================
    s = S('XZ', -10.3, 'sk_REF_board')
    s.poly([(-12.7, 17.15), (12.7, 17.15), (12.7, -9.45), (10.8, -10.25), (10.8, -17.15), (-12.7, -17.15)])
    for sx in (-1, 1): s.circle(sx * PEG_XY[0], PEG_XY[1], 2.0)
    board = new_body(ext(s, s.profile_with_loops(3), DN, 1.5, NB, 'REF_board'), 'REF_board')
    # corner fillets: R4 at the antenna end, R1 at the two USB-end corners
    def vedges(body, pts):
        col = adsk.core.ObjectCollection.create()
        for e in body.edges:
            a, b = e.startVertex.geometry, e.endVertex.geometry
            if abs(a.x - b.x) < 1e-6 and abs(a.z - b.z) < 1e-6:
                for (x, y) in pts:
                    if abs(a.x - x * 0.1) < 1e-4 and abs(a.z + y * 0.1) < 1e-4: col.add(e)
        return col
    for r, pts in ((4.0, [(-12.7, 17.15), (12.7, 17.15)]), (1.0, [(-12.7, -17.15), (10.8, -17.15)])):
        fi = root.features.filletFeatures.createInput()
        fi.addConstantRadiusEdgeSet(vedges(board, pts), V.createByReal(r * 0.1), True)
        root.features.filletFeatures.add(fi).name = 'REF_board_fillet_R%g' % r

    def ref_box(name, c, u0, v0, u1, v1, dirn, dist):
        s = S('XZ', c, 'sk_' + name); s.rect(u0, v0, u1, v1)
        new_body(ext(s, s.sk.profiles.item(0), dirn, dist, NB, name), name)
    ref_box('REF_usbc', -11.8, -4.6, -17.95, 4.6, -9.85, DN, 3.3)
    ref_box('REF_chip', -11.8, -3.9, -2.85, 3.9, 5.05, DN, 1.0)
    ref_box('REF_btn_boot', -11.8, -14.2, -16.25, -11.2, -11.25, DN, 3.0)
    ref_box('REF_btn_rst', -11.8, 8.5, -16.25, 12.0, -11.25, DN, 3.0)
    h = SW_HOUSING_W / 2
    ref_box('REF_sw_housing', 0.0, -h, -h, h, h, UP, SW_HOUSING_TOP)
    h = SW_LOWER_W / 2
    ref_box('REF_sw_lower', 0.0, -h, -h, h, h, DN, -SW_LOWER_BOT)
    s = S('XZ', SW_LOWER_BOT, 'sk_REF_sw_post'); s.circle(0, 0, 4.0)
    new_body(ext(s, s.sk.profiles.item(0), DN, 3.5, NB, 'REF_sw_post'), 'REF_sw_post')
    ref_box('REF_sw_pin1', SW_LOWER_BOT, -4.31, 2.39, -3.31, 2.69, DN, 3.3)
    ref_box('REF_sw_pin2', SW_LOWER_BOT, 2.04, 4.93, 3.04, 5.23, DN, 3.3)
    s = S('XZ', SW_HOUSING_TOP, 'sk_REF_sw_collar'); s.crect(5.6, 5.6)
    new_body(ext(s, s.sk.profiles.item(0), UP, STEM_BASE_TOP - SW_HOUSING_TOP, NB, 'REF_sw_collar'), 'REF_sw_collar')
    s = S('XZ', STEM_BASE_TOP, 'sk_REF_sw_cross'); s.rect(-2.0, -0.65, 2.0, 0.65); s.rect(-0.65, -2.0, 0.65, 2.0)
    new_body(ext(s, s.all_profiles(), UP, STEM_CROSS_TOP - STEM_BASE_TOP, NB, 'REF_sw_cross'), 'REF_sw_cross')

    log('REF done')
    # ================= CASE_SHELL =================
    s = S('XZ', Z_BOT, 'sk_CASE_SHELL'); s.crect(CASE_W, CASE_L, R_OUT)
    shell = new_body(ext(s, s.sk.profiles.item(0), UP, Z_TOP - Z_BOT, NB, 'CASE_SHELL'), 'CASE_SHELL')
    SH = [shell]
    s = S('XZ', Z_BOT, 'sk_cut_cavity'); s.crect(CAV_W, CAV_L, R_CAV)
    ext(s, s.sk.profiles.item(0), UP, SKIN_Z - Z_BOT, C, 'cut_cavity', SH)
    s = S('XZ', SKIN_Z, 'sk_cut_sw_hole'); s.crect(SW_CUT, SW_CUT)
    ext(s, s.sk.profiles.item(0), UP, Z_TOP - SKIN_Z, C, 'cut_sw_hole', SH)
    s = S('XZ', SKIN_Z, 'sk_cut_sw_relief'); s.crect(SW_RELIEF, SW_RELIEF)
    ext(s, s.sk.profiles.item(0), UP, WEB_Z - SKIN_Z, C, 'cut_sw_relief', SH)
    # four posts (board supports). The y>0 pair also carries pegs into the board mount holes.
    s = S('XZ', SKIN_Z, 'sk_posts')
    for sx in (-1, 1):
        for sy in (-1, 1): s.circle(sx * PEG_XY[0], sy * PEG_XY[1], POST_D)
    ext(s, s.all_profiles(), DN, SKIN_Z - POST_BOT, J, 'posts', SH)
    s = S('XZ', POST_BOT, 'sk_pegs')
    for sx in (-1, 1): s.circle(sx * PEG_XY[0], PEG_XY[1], PEG_D)
    ext(s, s.all_profiles(), DN, POST_BOT - PEG_BOT, J, 'pegs', SH)
    # openings
    s = S('XY', -CASE_L / 2, 'sk_cut_usbc'); s.rect(-USB_W / 2, USB_Z0, USB_W / 2, USB_Z1, R_USB)
    ext(s, s.sk.profiles.item(0), UP, 2.5, C, 'cut_usbc', SH)
    s = S('YZ', -CASE_W / 2, 'sk_cut_boot_window'); s.rect(BOOT_Y - BOOT_HALF, BOOT_Z0, BOOT_Y + BOOT_HALF, BOOT_Z1, R_BOOT)
    ext(s, s.sk.profiles.item(0), UP, 2.5, C, 'cut_boot_window', SH)
    s = S('YZ', CASE_W / 2, 'sk_cut_rst_hole'); s.circle(BOOT_Y, RST_Z, RST_D)
    ext(s, s.sk.profiles.item(0), DN, 2.5, C, 'cut_rst_hole', SH)
    for side, dirn in ((-1, UP), (1, DN)):
        s = S('YZ', side * CASE_W / 2, 'sk_cut_latch_win')
        for (y0, y1) in WIN_Y: s.rect(y0, WIN_Z0, y1, WIN_Z1)
        ext(s, s.all_profiles(), dirn, 2.5, C, 'cut_latch_win', SH)
    s = S('XZ', Z_TOP, 'sk_cut_shroud_peg_holes')
    for sx in (-1, 1):
        for sy in (-1, 1): s.circle(sx * SHROUD_PEG_XY[0], sy * SHROUD_PEG_XY[1], SHROUD_PEG_HOLE_D)
    ext(s, s.all_profiles(), DN, SHROUD_PEG_HOLE_DEPTH, C, 'cut_shroud_peg_holes', SH)

    # ================= BOTTOM_CAP =================
    s = S('XZ', Z_BOT, 'sk_BOTTOM_CAP'); s.crect(CAP_W, CAP_L, R_CAP)
    cap = new_body(ext(s, s.sk.profiles.item(0), UP, CAP_T, NB, 'BOTTOM_CAP'), 'BOTTOM_CAP')
    CP = [cap]
    # v0.12: no cap bosses. The pegs now stand 3.0 below the board and are heat-staked, so the cap must stay clear of them.
    s = S('XZ', Z_BOT + CAP_T, 'sk_cap_usb_relief'); s.rect(-5.0, -17.5, 5.0, -9.5)
    ext(s, s.sk.profiles.item(0), DN, 0.4, C, 'cap_usb_relief', CP)
    s = S('XZ', Z_BOT, 'sk_cap_latch_slots')
    for sx in (-1, 1):
        for (x0, y0, x1, y1) in SLOT_RECTS:
            a, b = sorted((sx * x0, sx * x1)); s.rect(a, y0, b, y1)
    ext(s, s.all_profiles(), UP, CAP_T, C, 'cap_latch_slots', CP)
    for sx in (-1, 1):
        for (y0, y1) in HOOKS_Y:
            s = S('XY', y0, 'sk_cap_hook')  # profile in x/z, extruded along +y
            s.poly([(sx * HOOK_BASE_X, HOOK_Z0), (sx * HOOK_TIP, HOOK_Z0), (sx * HOOK_TIP, HOOK_CATCH_Z),
                    (sx * HOOK_ARM_X, HOOK_Z1), (sx * HOOK_BASE_X, HOOK_Z1)])
            ext(s, s.sk.profiles.item(0), UP, y1 - y0, J, 'cap_hook', CP)

    # ================= KEYCAP =================
    s = S('XZ', KEY_PLATE_Z0, 'sk_KEYCAP'); s.crect(KEY_W, KEY_W, KEY_R)
    key = new_body(ext(s, s.sk.profiles.item(0), UP, KEY_PLATE_T, NB, 'KEYCAP'), 'KEYCAP')
    KP = [key]
    s = S('XZ', KEY_SKIRT_Z0, 'sk_keycap_skirt'); s.crect(KEY_W, KEY_W, KEY_R); s.crect(KEY_W - 2 * KEY_WALL, KEY_W - 2 * KEY_WALL, KEY_R - KEY_WALL)
    ext(s, s.profile_with_loops(2), UP, KEY_PLATE_Z0 - KEY_SKIRT_Z0, J, 'keycap_skirt', KP)
    s = S('XZ', KEY_SLEEVE_Z0, 'sk_keycap_sleeve'); s.circle(0, 0, KEY_SLEEVE_D)
    ext(s, s.sk.profiles.item(0), UP, KEY_PLATE_Z0 - KEY_SLEEVE_Z0, J, 'keycap_sleeve', KP)
    s = S('XZ', KEY_SLEEVE_Z0, 'sk_keycap_cross')
    s.rect(-KEY_CROSS_L / 2, -KEY_CROSS_W / 2, KEY_CROSS_L / 2, KEY_CROSS_W / 2)
    s.rect(-KEY_CROSS_W / 2, -KEY_CROSS_L / 2, KEY_CROSS_W / 2, KEY_CROSS_L / 2)
    ext(s, s.all_profiles(), UP, KEY_CROSS_TOP - KEY_SLEEVE_Z0, C, 'keycap_cross', KP)

    # ================= SHROUD =================
    s = S('XZ', Z_TOP, 'sk_SHROUD'); s.crect(CASE_W, CASE_L, R_OUT)
    shroud = new_body(ext(s, s.sk.profiles.item(0), UP, SH_BASE_T, NB, 'SHROUD'), 'SHROUD')
    SR = [shroud]
    s1 = S('XZ', Z_TOP + SH_BASE_T, 'sk_shroud_taper_base'); s1.crect(CASE_W, CASE_L, R_OUT)
    s2 = S('XZ', SH_TOP_Z, 'sk_shroud_taper_top'); s2.crect(SH_TOP_W, SH_TOP_W, SH_TOP_R)
    li = root.features.loftFeatures.createInput(NB)
    li.loftSections.add(s1.sk.profiles.item(0)); li.loftSections.add(s2.sk.profiles.item(0))
    lf = root.features.loftFeatures.add(li); lf.name = 'shroud_taper'
    tmp = lf.bodies.item(0)
    tools = adsk.core.ObjectCollection.create(); tools.add(tmp)
    ci = root.features.combineFeatures.createInput(shroud, tools); ci.operation = J
    root.features.combineFeatures.add(ci).name = 'shroud_join_taper'
    s = S('XZ', SH_TOP_Z, 'sk_shroud_bore'); s.crect(SH_BORE_W, SH_BORE_W, SH_BORE_R)
    ext(s, s.sk.profiles.item(0), DN, SH_TOP_Z - Z_TOP, C, 'shroud_bore', SR)
    s = S('XZ', Z_TOP, 'sk_shroud_pegs')
    for sx in (-1, 1):
        for sy in (-1, 1): s.circle(sx * SHROUD_PEG_XY[0], sy * SHROUD_PEG_XY[1], SHROUD_PEG_D)
    ext(s, s.all_profiles(), DN, SHROUD_PEG_LEN, J, 'shroud_pegs', SR)

    # ================= report, hide REF, optional STL export =================
    bodies = [b for b in root.bRepBodies]
    c = adsk.core.ObjectCollection.create()
    for b in bodies: c.add(b)
    ii = des.createInterferenceInput(c); ii.areCoincidentFacesInterference = False
    res = des.analyzeInterference(ii)
    out = []
    for b in bodies:
        bb = b.boundingBox
        out.append('%s vol=%.1f z[%.2f,%.2f]' % (b.name, b.volume * 1000, bb.minPoint.y * 10, bb.maxPoint.y * 10))
        if b.name.startswith('REF'): b.isLightBulbOn = False
    out.append('interferences %d' % res.count)
    for i in range(res.count):
        out.append('  %s x %s' % (res.item(i).entityOne.name, res.item(i).entityTwo.name))
    if EXPORT_DIR:
        for n in ('CASE_SHELL', 'BOTTOM_CAP', 'KEYCAP', 'SHROUD'):
            b = [x for x in bodies if x.name == n][0]
            o = des.exportManager.createSTLExportOptions(b, EXPORT_DIR + '/OneButton_' + n + '.stl')
            o.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
            des.exportManager.execute(o)
    print('\n'.join(out))
