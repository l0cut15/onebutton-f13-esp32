# OneButton case: design spec (v0.8, 27 Oct 2026)

Purpose: single source of truth for the 3D printed case of a one-button USB voice-dictation controller. Written so the model can be rebuilt in Fusion, or any other CAD tool, from this file alone. Units: mm. Status flags: [M] measured by Brett, [D] taken from a drawing or photo (about +/-0.5 mm), [N] nominal from a standard or datasheet (verify), [P] placeholder.

## 1. Product
- Board: Wemos/Lolin ESP32-S2 Mini, USB-C on one short end. Switch: Kailh Box Navy (MX compatible, clicky). Switch wired between GPIO17 and GND with two flexible wires soldered directly to the switch pins and the board pads (no header).
- Keycap and shroud printed. PLA first, ABS later for the keycap. Printer nozzle 0.4 mm (assumed). Material shrinkage is compensated in the slicer, so the model uses nominal dimensions.
- Snap-fit assembly, no screws. Prototype and iterate.

## 2. Coordinate system
- Origin: centre of the switch in plan (also the centre of the case and board). Z = 0 is the plate seat, the top of the 1.5 web where the switch flange rests.
- +X to the right and +Y towards the antenna end when looking down from the top. USB-C is at -Y. BOOT button is on -X, RST on +X. Z positive up, case bottom at -17.1.

## 3. Components (build and print separately)
| # | Name | Printed | Description |
|---|------|---------|-------------|
| 1 | CASE_SHELL | yes | Outer body, solid top block, switch pocket and web, peg posts, USB-C opening, BOOT window, RST hole, cap latch windows |
| 2 | BOTTOM_CAP | yes | Floor, bosses that hold the board from below, in-plane snap latch arms |
| 3 | KEYCAP | yes | 24 x 24 mm, R4, 3.0 plate, 1.2 skirt, dia 5.6 sleeve with 4.15 x 1.40 cross socket |
| 4 | SHROUD | yes | 30 x 39 bezel, Z 6.0 to 13.5, 24.6 square bore (R4.3) guiding the keycap, four dia 3.0 x 3.0 pegs into dia 3.2 holes in the case top, glued in v1 |
| R | REF_ELECTRONICS | no | Envelope models of the Box Navy and S2 Mini for interference checks only |

Build order: CASE_SHELL, then BOTTOM_CAP, then KEYCAP, then SHROUD. Reference models before CASE_SHELL.

## 4. Parameters
| Name | Value | Status | Note |
|------|-------|--------|------|
| case_w x case_l | 30.0 x 39.0 | design | outer, vertical corner radius 5 |
| wall_t | 2.0 | design | |
| cavity | 26.0 x 35.0, corner R1.5 | design | = board 25.4 x 34.3 + 0.3 clearance per side. R3 clipped the nearly square USB-end board corners, so R1.5 |
| z_bottom | -17.1 | design | case bottom |
| h_up | 6.0 | [P] | plate seat to housing top. Measure |
| skin underside | -3.0 | design | |
| web_t | 1.5 | design | web occupies Z 0 to -1.5 |
| sw_cut | 14.0 square | [N] | tune with a 13.9 to 14.2 coupon |
| sw_pocket | 16.0 square | design | counterbore Z 0 to +h_up, relief Z -1.5 to -3.0 (housing flange about 15.6) |
| board thickness | 1.5 | [M] | |
| board outline | 34.3 x 25.4 | [M] | top corners about R4, 1.9 deep step on the RST-side edge over the last 7.7 mm at the USB end [D] |
| board Z | back face -10.3, underside -11.8 | design | flipped, components down |
| mount holes | 2 x dia 2.0, 20.4 apart, 2.5 from side edges, 3.3 from the antenna end | [D] | at X +/-10.2, Y +13.85 |
| peg posts | dia 4.0 from Z -3 to -10.3, peg dia 1.8 to Z -11.5 | design | test on a coupon, 1.8 into 2.0 may bind |
| USB-C | 9.2 wide, 3.3 tall, overhangs the board end about 0.5 to 1 | [D][N] | opening 9.8 x 4.1, Z -11.4 to -15.5, corner R1.0. Plug overmold counterbore NOT in v1 (would leave under 0.6 mm of wall below it): test the real cable first |
| side buttons | centres 3.4 from the USB end edge, about 5 long, height TBD (btn_h placeholder 3.0) | [D][P] | at Y -13.75 |
| BOOT plunger | overhangs the board by 1.5 | [M] | window through the -X wall, 6.5 long, height btn_h + 0.6 |
| RST access | dia 2.0 hole through the +X wall at the button height | design | |
| switch pins | at (-3.81, +2.54) and (+2.54, +5.08) from the switch centre (MX standard) | [N] | check orientation against the real part |
| switch depth | housing bottom Z -5.0, pin tips -8.3, solder zone to -9.8 | [N] | measure flange underside to pin tips, nominal 6.8 |
| pad positions | GPIO17 and GND on the right header (board top view), inner column, 14.2 and 11.7 from the USB end, 4.0 from the right edge | [D] | in the flipped case this is the -X side |
| cap | floor 2.0 (Z -17.1 to -15.1), outline 25.6 x 34.6 R1.3 (0.2 clearance), 4 snap latches (2 per long side) | design | arms lie in the XY plane of the floor so layer lines run along the beam. Arm 1.4 wide, slot 1.2, hook protrusion 0.6, hook 3.6 long, arm A free end y=10.0, arm B free end y=-8.0 (fixed ends meet near y=1). Hooks rise to Z -13.7 into windows (y 5.5 to 10.5 and -8.5 to -3.5, Z -15.2 to -13.2) in the long walls. Release by pressing a hook in through its window |
| cap bosses | dia 4.0 at X +/-10.2, Y 13.85, top at Z -11.9 | design | board is held between post ends (Z -10.3) and boss tops with 0.1 gap |
| cap USB relief | 10 wide, 0.4 deep under the connector | design | |
| keycap Z | underside at rest Z 10.0 (case top + 4.0), plate Z 12.0 to 15.0, cap top Z 15.0 | design | full press leaves 0.4 above the case top |
| stem (REF) | shoulder 3.6 above housing top, cross 4.0 tall, cross top Z 13.6 | [P] | earlier draft wrongly had the stem only 3.6 high. Measure |
| keycap travel | 3.6, rest gap 4.4 | [N] | |

## 5. CASE_SHELL features
1. Outer extrude Z -17.1 to +h_up, vertical edges R5.
2. Cavity, open at the bottom: 26.0 x 35.0, R1.5, from Z -17.1 to -3.0.
3. Switch: 14.0 square through-hole Z +h_up to -3.0, 16.0 square pocket Z +h_up to 0, 16.0 square relief Z -1.5 to -3.0. Result: a 1.0 mm ring of web (Z 0 to -1.5) between 14.0 and 16.0.
4. Peg posts at X +/-10.2, Y +13.85.
5. Wire bay: no feature, it is the open cavity beside the switch (6 mm per side).
6. USB-C opening in the -Y wall, centred on X 0.
7. BOOT window in the -X wall at Y -13.75, Z about -11.9 to -15.5. RST hole in the +X wall at the same Y.
8. Four cap latch windows through the long walls, 5.0 long: Y 5.5 to 10.5 and Y -8.5 to -3.5, Z -15.2 to -13.2. Built (v0.3).
9. Shroud interface: four dia 3.2 x 3.5 holes in the case top at X +/-11, Y +/-15.
Print orientation: top face down, open bottom up. The 1 mm web ledge is an acceptable short overhang.

## 6. Design intent and risks
- Raised top block holds the housing on four sides, so the 1.5 web only carries pull-out.
- A single centred stem cannot control a large cap: it rocks. Hence the smaller cap with a guiding shroud bore.
- PLA snaps are brittle: long in-plane beams, small deflection, test fit before printing the final cap. Boot and reset are reachable through the walls, so the cap does not need to be removed to flash.
- Wiring: 2 wires, about 15 mm each plus slack, soldered before the board goes in.

## 7. Open items
1. Measure h_up, switch flange-to-pin-tips, btn_h, USB-C height.
2. Keycap size and shape, shroud height and bore, shroud attachment (v1: pegs plus glue, then snaps).
3. Print a coupon first: switch cutout 13.9 to 14.2, peg dia 1.8, keycap stem socket.
4. Mirror check: boot side is -X in the flipped case.

## 8. Decision log
- 27 Oct: GPIO changed from 5 to 17 (next to GND). Board flipped and centred. BOOT overhang handled by a wall window, not by offsetting the board. Raised top block around the switch housing. Smaller keycap with separate shroud. Four printed components plus a reference component.
- 27 Oct (build): the Fusion document is a Part Design file, which allows only one component. Components are therefore separate named bodies in one file: CASE_SHELL, BOTTOM_CAP, KEYCAP, SHROUD, and REF_* (reference). Export each body to STL separately.
- 27 Oct (build): cavity corner R3 changed to R1.5 and USB opening corner R1.5 changed to R1.0 after an interference check against the reference models (the reference board now has its real corner radii, R4 at the antenna end, R1 at the USB end).

## 9. Fusion build notes
- The document is Y-up. Spec axes map to Fusion as (x, y, z) -> (x, z, -y): spec Z (up) is Fusion Y, spec +Y (antenna end) is Fusion -Z, so the USB-C end faces the front view.
- Build is driven by a Python script with the parameters at the top (H_UP, BTN_H, CASE_W, CASE_L, WALL, Z_BOT, SKIN_Z, SW_CUT, SW_POCKET, WEB_T, USB_*, BTN_Y, WIN_L, R_OUT, R_CAV, R_USB). Sketch sizes come from the script, not from Fusion user parameters, so a dimension change means re-running the script after deleting the shell features.
- Every cut targets CASE_SHELL only, so reference bodies are never cut. Interference is checked against all REF_* bodies after each build (result for v0.2: 0 interferences).
- CASE_SHELL timeline features: CASE_SHELL, cut_cavity, cut_sw_pocket, cut_sw_hole, cut_sw_relief, post_L/R, peg_L/R, cut_usbc, cut_boot_window, cut_rst_hole. Shell volume 11.57 cm3, outer 30.0 x 39.0 x 23.1.

## 10. Status
- Done (v0.3): REF_ELECTRONICS, CASE_SHELL (with latch windows and shroud peg holes), BOTTOM_CAP, KEYCAP, SHROUD. Interference check across all bodies: 0.
- Overall height: case 23.1 (Z -17.1 to +6.0), shroud top Z 13.5, keycap top Z 15.0, so about 32.1 mm total.
- Next: measurements (h_up, stem and cross heights, btn_h, USB-C height), a fit coupon print (switch cutout, peg fit, cross socket, one latch), then a full print.
- 27 Oct (v0.3): keycap 24 mm square with a guiding shroud (glue for v1). Cap latch windows cut in the shell. Reference stem corrected to shoulder 3.6 plus cross 4.0.

## 11. Exports and first print (v0.4)
STL files (mm, high refinement, in design orientation, Y-up) in this folder: OneButton_CASE_SHELL.stl, OneButton_BOTTOM_CAP.stl, OneButton_KEYCAP.stl, OneButton_SHROUD.stl. Reference bodies are not exported. Slicer handles shrinkage compensation.

Suggested orientation (lay each part flat on the named face):
| Part | Face on the bed | Why |
|------|-----------------|-----|
| CASE_SHELL | top face (Z +6.0), open bottom up | clean top, pocket and web ledge print as a 1 mm overhang, no supports expected. Check the preview for the USB-C opening and the windows (small bridges) |
| BOTTOM_CAP | floor (Z -17.1) | latch arms lie in the XY plane so layers run along the beam; hooks and bosses point up |
| KEYCAP | top face (Z +15.0) | cross socket and sleeve print upright and open at the top |
| SHROUD | top face (Z +13.5), pegs up | bore prints vertical |

Suggested settings (PLA, 0.4 nozzle): 0.2 layer, at least 3 perimeters (the latch arms are 1.4 wide), 15 to 20 percent infill, no supports first.

Print order: KEYCAP first (small, tests the cross socket on the real switch), then BOTTOM_CAP, then CASE_SHELL, then SHROUD.

First-fit checklist, record the result of each so the parameters can be tuned:
1. Keycap cross socket on the switch stem: too tight or loose? Does the sleeve clear the stem collar? (CROSS_L 4.15, CROSS_W 1.40, SLEEVE_D 5.6)
2. Switch into the shell: does the 14.0 cutout hold, do the latches catch under the 1 mm web? (SW_CUT, SW_POCKET)
3. Board: does it drop onto the two pegs (dia 1.8 into the dia 2.0 holes) and sit level?
4. USB-C: does your cable seat fully in the 9.8 x 4.1 opening? (plug overmold counterbore not in v1)
5. BOOT window and RST hole: can you press both?
6. Cap: does it snap in and hold, and release by pressing a hook through its window? Any arm cracking?
7. Shroud: peg fit in the holes, then glue. Does the keycap travel freely, rock, or rub in the bore? (BORE_CLR 0.6)
8. Measure the real switch and record: housing height above the plate seat (placeholder 6.0), stem shoulder and cross heights (3.6 and 4.0), side-button height (3.0), USB-C height (3.3).


## 12. Revision v0.5 (post first print) : supersedes earlier sections where they conflict

Changes from print 1 feedback:
1. No material above plate zero. Case top is Z 0 (case Z -17.1..0, height 17.1). The raised block and the 16 mm pocket above the plate are removed. cut_sw_hole (14.0 square) and cut_sw_relief (16.0 square, Z -3..-1.5) remain. H_UP now only sets housing height above the case top and drives keycap and shroud heights.
2. Shroud is tapered (cone-like). Vertical base 1.5 mm (30 x 39, R5) at Z 0..1.5, loft to 28 x 28, R6 at Z 13.5. Bore is straight 24.6 square, R4.3, Z 0..13.5 (keycap 24 x 24 plus 0.6 clearance). Alternative if a funnel is wanted instead: chamfer the bore mouth at Z 13.5.
3. Shroud pegs: 3.0 dia x 2.0 long into 3.2 dia x 2.2 deep holes at (+-11, +-15) on the Z 0 top face. Depth is limited by the 3.0 mm top skin.
4. Two extra plain posts at (+-10.2, -13.85), dia 4.0, Z -3..-10.3, supporting the board behind the USB-C. They touch the board's flat back face. No pegs, no through holes. No matching cap bosses: the board underside there carries the BOOT and RST buttons and USB-C shell.
5. Overall height about 32.1 mm (case 17.1, shroud top Z 13.5, keycap top Z 15.0).

Print notes: shroud printed top down, taper overhang about 25 degrees from vertical, acceptable without support. STLs re-exported (CASE_SHELL, BOTTOM_CAP, KEYCAP, SHROUD).

Open measurements: switch housing height above plate seat (placeholder 6.0), stem shoulder and cross heights (3.6 / 4.0), side button height (3.0), USB-C height (3.3).


## 13. Revision v0.6 : switch datasheet applied, keycap and shroud resized (supersedes section 12 where they conflict)

Switch REF now follows the Kailh Box Navy datasheet, not estimates: housing top 6.2 above plate (11.2 housing top to body bottom, 5.0 below plate), housing 15.6 square, stem 3.8 above housing top (stem top Z 10.0, cross 3.6 tall on a 0.2 base), cross arms 4.0 long x 1.3 wide. Stem is flush with the housing top when fully pressed.

Changes:
1. SHROUD height reduced by 2.0: top at Z 11.5 (was 13.5). Taper loft 30 x 39 R5 at Z 1.5 to 28 x 28 R6 at Z 11.5. Bore 24.6 square R4.3, Z 0..11.5.
2. KEYCAP grown 0.25 per edge: 24.5 x 24.5, corner R4.25 (was 24.0, R4.0). Gap to bore is 0.05 per edge, very tight for FDM PLA. If it binds, reduce to 24.2 to 24.3.
3. KEYCAP deeper by 1.5: plate Z 10.0..13.0 (top 1.5 above shroud top), skirt Z 6.5..10.0 (wall 1.2), sleeve dia 5.6 Z 6.6..10.0, cross socket 4.15 x 1.40 up to Z 10.2 (0.2 above stem top at rest).
4. Overall height about 30.1 (case 17.1 below Z 0, keycap top Z 13.0).
5. Interference check across all bodies: 0.

Open risk: with the stem only 3.8 above the housing, the sleeve bottom (Z 6.6) sits 0.4 above the housing top (6.2) at rest. Full 3.6 travel needs the housing top to accept the sleeve (stem recess), otherwise the sleeve bottoms out after about 0.4. Check on the physical switch with the first print. Fallback: shorten the sleeve and engage only 1.5 to 2.0 of the cross.


## 14. Revision v0.7 : keycap tolerance relaxed

KEYCAP outer relaxed from 24.5 to 24.3 square, corner R4.15 (skirt wall 1.2 unchanged). Gap to the 24.6 bore is now 0.15 per edge (was 0.05). Everything else as v0.6. If it still binds after shrinkage compensation, go to 24.1.


## 15. Revision v0.8 : deeper snap hooks

Cap hook tip moved outward from x +-13.4 to +-14.0. The hook now sits 1.0 mm into the 2.0 mm wall windows (was 0.4), leaving the window opening through the wall. Hook profile: base Z -15.1, catch face 0.4 tall, ramp up to arm outer face (12.8) at Z -13.7. Hook length 3.6 along the arm, two arms per side.
Assembly deflection is now 1.0 mm against a 1.2 mm flex slot beside each arm, arm width 1.4, about 19 mm long (estimated strain under 1 percent). PLA is brittle: if an arm cracks, print the cap with the arms along the layer lines as now, or reduce the tip to 13.8.
