# OneButton case: design spec (v0.12 consolidated, 27 Oct 2026)

Single source of truth for the 3D printed case of a one-button USB voice-dictation controller. Written so the model can be rebuilt in Fusion, or any other CAD tool, from this file alone. Units: mm. Flags: [M] measured by Brett, [D] taken from a drawing or photo (about +/-0.5), [N] nominal from a datasheet or standard (verify), [P] placeholder. The previous revision-by-revision spec is kept as OneButton_design_spec_v0.8_history.md.

Files in this folder:
- OneButton_design_spec.md: this file (current state).
- onebutton_build.py: Fusion 360 Python script that rebuilds all bodies from the parameters in section 4. See section 9.
- OneButton_CASE_SHELL.stl, OneButton_BOTTOM_CAP.stl, OneButton_KEYCAP.stl, OneButton_SHROUD.stl: current exports (v0.12 geometry, design orientation, mm). CASE_SHELL (USB opening, longer pegs), BOTTOM_CAP (bosses removed) and SHROUD (height) changed after v0.8.

Changelog since v0.8: v0.12 pegs through the board, 3.0 proud, for heat staking, cap bosses removed; v0.9 consolidated spec and added onebutton_build.py; v0.10 USB-C opening 5.5 tall (Z -16.2 to -10.7); v0.11 shroud top 11.5 to 8.5. Model volumes (v0.12): CASE_SHELL 6240.1, BOTTOM_CAP 1673.7, KEYCAP 2138.6, SHROUD 3413.3 mm3.

## 1. Product
- Board: Wemos/Lolin ESP32-S2 Mini, USB-C on one short end. Switch: Kailh Box Navy (MX compatible, clicky). Switch wired between GPIO17 and GND with two flexible wires soldered straight to the switch pins and the board pads (no header). GPIO is changeable in firmware.
- Keycap and shroud printed. PLA first, ABS later for the keycap. 0.4 nozzle (assumed). Shrinkage compensation is done in the slicer, the model uses nominal dimensions.
- Snap-fit, no screws, glue allowed for the shroud in v1. Low-cost prototypes, iterate.

## 2. Coordinate system
- Origin: switch centre in plan (also case and board centre). Z = 0 is the top face of the case (and of the 1.5 mm web seat for the switch flange). +X right, +Y towards the antenna end, USB-C at -Y. BOOT button on -X, RST on +X (board flipped, components down). Z up, case bottom at -17.1.
- Fusion mapping (document is Y-up): spec (x, y, z) -> Fusion (x, z, -y).

## 3. Components
| Name | Printed | Description |
|------|---------|-------------|
| CASE_SHELL | yes | 30 x 39 body, Z -17.1 to 0, open at the bottom. Switch hole and web, board posts and pegs, USB-C opening, BOOT window, RST hole, cap latch windows, shroud peg holes |
| BOTTOM_CAP | yes | 2.0 floor, four in-plane snap arms with hooks (no board bosses since v0.12) |
| KEYCAP | yes | 24.3 square, plate Z 10..13, skirt down to Z 6.5, cross socket sleeve |
| SHROUD | yes | tapered bezel Z 0..8.5 with a straight bore guiding the keycap, four pegs into the case top, glued |
| REF_* | no | envelope models of board, USB-C, chip, side buttons and Box Navy, for interference checks only |

Total height about 30.1 mm (unchanged: the keycap sets it) (case 17.1 below Z 0, keycap top at Z 13.0).

## 4. Parameters (current)
### Case shell
| Name | Value | Note |
|------|-------|------|
| outer | 30.0 x 39.0, vertical corner R5, Z -17.1 to 0 | |
| wall | 2.0 | |
| cavity | 26.0 x 35.0, R1.5, Z -17.1 to -3.0 | board 25.4 x 34.3 + 0.3 clearance per side. R3 clipped the USB-end board corners |
| top skin | Z -3.0 to 0 (3.0 thick) | |
| switch hole | 14.0 square, Z -3.0 to 0 | [N] tune with a 13.9 to 14.2 coupon |
| relief | 16.0 square, Z -3.0 to -1.5 | leaves a 1.5 web at Z -1.5 to 0, 1.0 mm ledge ring, for the switch latches |
| posts | dia 4.0, Z -3.0 to -10.3, at X +/-10.2, Y +13.85 and Y -13.85 | the Y -13.85 pair supports the board behind the USB-C (plain, no peg) |
| pegs | dia 1.8, Z -10.3 to -14.8, at X +/-10.2, Y +13.85 only | through the board's dia 2.0 mount holes [D] (board underside Z -11.8) and 3.0 proud. Heat-stake the protruding plastic with a hot tool to hold the board. Do this before fitting the cap. Test fit 1.8 into 2.0 |
| USB-C opening | 9.8 wide x 5.5 tall, Z -16.2 to -10.7 (centred on the connector, Z -13.45), corner R1.0, through the -Y wall | connector 9.2 x 3.3 [D][N]. Was 4.1 tall: some cables with thick plastic collars did not fit. Wall left under the opening is 0.9. Plug overmold counterbore not in v1 |
| BOOT window | -X wall, Y -17.0 to -10.5 (centre -13.75, 6.5 long), Z -15.2 to -11.6, corner R0.5 | plunger overhangs the board by 1.5 [M] |
| RST hole | dia 2.0 through the +X wall at Y -13.75, Z -13.3 | |
| cap latch windows | through both long walls, Y 5.5 to 10.5 and -8.5 to -3.5, Z -15.2 to -13.2 | |
| shroud peg holes | dia 3.2 x 2.2 deep from the top face at X +/-11, Y +/-15 | depth limited by the 3.0 skin |
### Board (reference)
34.3 x 25.4 x 1.5 [M] (X +/-12.7, Y +/-17.15), back face Z -10.3, underside Z -11.8. Antenna-end corners R4, USB-end corners R1, a step at +X: the outline goes (12.7,-9.45) to (10.8,-10.25) then down to the USB end [D]. Mount holes 2 x dia 2.0 at X +/-10.2, Y +13.85 [D]. Chip 7.8 x 7.9 x 1.0 (Y -2.85 to 5.05) under the board. USB-C 9.2 x 8.1 x 3.3 (Y -17.95 to -9.85). Side buttons 5.0 long, 3.0 tall, centred Y -13.75: BOOT X -14.2 to -11.2, RST X 8.5 to 12.0 [D][P]. Pads: GPIO17 and GND on the header column nearest the BOOT side [D].
### Switch (reference, Kailh Box Navy datasheet)
Housing 15.6 square, top 6.2 above Z 0 (11.2 housing top to body bottom). Lower body 14.0 square, Z -5.0 to 0. Stem protrudes 3.8 above the housing top: top at Z 10.0, cross arms 4.0 x 1.3, 3.6 tall on a 0.2 base. Pins at (-3.81, 2.54) and (2.54, 5.08), tips Z -8.3 [N]. Stem is flush with the housing top when fully pressed [M, Brett]. Travel 3.6.
### Bottom cap
| Name | Value |
|------|-------|
| outline | 25.6 x 34.6, R1.3, floor Z -17.1 to -15.1 (0.2 clearance to the cavity) |
| bosses | none (v0.12). The board is held by the heat-staked pegs, and the pegs stand 3.0 below the board, which leaves 0.3 above the cap floor (Z -15.1) before staking. The earlier dia 4.0 bosses at the peg positions would collide with the pegs |
| USB relief | 10 wide x 8 long (Y -17.5 to -9.5), 0.4 deep into the floor top |
| snap arms | four, two per long side, in the XY plane of the floor so layer lines run along the beam. Arm 1.4 wide (X 11.4 to 12.8), flex slot 1.2 on the inside. Arm A Y 1.4 to 10.0, arm B Y -8.0 to 0.6, joined to the floor between Y 0.6 and 1.4. Slot rectangles per side: X 10.2..11.4 / Y 1.4..10.0, X 10.2..12.9 / Y 10.0..11.2, X 10.2..11.4 / Y -8.0..0.6, X 10.2..12.9 / Y -9.2..-8.0 |
| hooks | on arm tops, Y 6.2 to 9.8 and -7.8 to -4.2. Profile (x, z): (11.4,-15.1), (14.0,-15.1), (14.0,-14.7), (12.8,-13.7), (11.4,-13.7). Tip 14.0 puts the hook 1.0 mm into the 2.0 wall windows. Assembly deflection 1.0 against the 1.2 slot. Release: press a hook in through its window |
### Keycap
| Name | Value |
|------|-------|
| outer | 24.3 square, corner R4.15. Bore is 24.6, so 0.15 per edge. If it still binds go to 24.1 |
| plate | Z 10.0 to 13.0 (4.5 above the shroud top at rest, plate underside 1.5 above it) |
| skirt | wall 1.2, Z 6.5 to 10.0, inner corner R2.95 |
| sleeve | dia 5.6, Z 6.6 to 10.0 |
| cross socket | 4.15 x 1.40 arms, Z 6.6 to 10.2 (0.2 above the stem top at rest) |
### Shroud
| Name | Value |
|------|-------|
| base | 30 x 39 R5, Z 0 to 1.5 |
| taper | loft from 30 x 39 R5 at Z 1.5 to 28 x 28 R6 at Z 8.5 (cone-like, about 38 degrees from vertical on the long side) |
| bore | 24.6 square R4.3, straight, Z 0 to 8.5 |
| pegs | dia 3.0 x 2.0 long below Z 0 into the case top holes, glued |
Alternative to the tapered outside: a chamfered funnel at the bore mouth.

## 5. Design intent and risks
- A single centred stem cannot control a large cap, it rocks. Hence a cap with a guiding bore.
- PLA snaps are brittle: long in-plane beams, small deflection. The 1.0 mm hook engagement is a trade between hold and cracking. If an arm cracks reduce the tip to 13.8.
- BOOT and RST are reachable through the walls, so the cap does not have to come off to flash.
- Wiring: two wires about 15 mm each plus slack, soldered before the board goes in.
- Open risk (stem and travel): with the stem 3.8 above the housing, the sleeve bottom (Z 6.6) sits only 0.4 above the housing top (6.2) at rest. Full 3.6 travel needs the housing top to accept the sleeve, otherwise the cap bottoms out after about 0.4. Check on the real switch. Fallback: shorten the sleeve and engage 1.5 to 2.0 of the cross.

## 6. Print notes (PLA, 0.4 nozzle, 0.2 layer, 3+ perimeters, 15 to 20 percent infill)
| Part | Face on the bed |
|------|-----------------|
| CASE_SHELL | top face down (flip the exported orientation 180), open bottom up. Web ledge is a 1.0 mm overhang, check bridges over the USB opening and windows |
| BOTTOM_CAP | floor down, as exported |
| KEYCAP | plate down (flip), skirt up |
| SHROUD | top face down (flip), pegs up. Taper about 25 degrees from vertical, no support expected |
Print order: KEYCAP, BOTTOM_CAP, CASE_SHELL, SHROUD.

## 7. Open items
1. Measure on the real parts: stem seating and travel against the sleeve, side-button height (3.0 placeholder), USB-C height (3.3), switch pin tips.
2. Record print results: cap snap hold and cracking, keycap-to-bore fit at 0.15 per edge, cross socket fit, peg fits, USB cable fit, BOOT and RST access.
3. Optional: USB-C plug overmold counterbore, shroud snaps instead of glue, ABS keycap.

## 8. Decision log
- GPIO moved from 5 to 17 (next to GND). Board flipped and centred. BOOT overhang handled by a wall window, not by offsetting the board.
- Fusion Part Design allows one component, so components are named bodies in the root: CASE_SHELL, BOTTOM_CAP, KEYCAP, SHROUD, REF_*. Each is exported to STL separately.
- Cavity corner R3 became R1.5 and USB opening corner R1.5 became R1.0 after an interference check against REF bodies (REF board has its real corner radii).
- Print 1 feedback: removed the raised block above Z 0 (the sides are angled in, no material above the plate is needed), added the two plain posts behind the USB-C, tapered the shroud.
- Switch REF replaced by datasheet values (Brett: use the diagram, his figures were approximations). Shroud 2 mm lower, keycap deeper by 1.5 and wider (24.0 -> 24.5 -> 24.3 after relaxing the fit), cap hooks 0.4 -> 1.0 deep.

- v0.10: USB-C opening raised to 5.5 tall (Z -16.2 to -10.7) for cables with thick plastic collars. Cap and board unaffected (the cap ends at Y -17.3, the collar zone is at Y -17.95 and beyond).
- v0.11: shroud 3 mm lower (top Z 8.5). Keycap unchanged, so at rest the skirt (Z 6.5 to 10.0) overlaps the bore by only 2.0 mm and the keycap plate stands 4.5 above the shroud. The taper is steeper (loft over 7 mm instead of 10), still printed top-down.
- v0.12: pegs lengthened to Z -14.8 (4.5 long, dia 1.8) to pass through the board and stand 3.0 proud for heat staking. Cap bosses at the peg positions removed. Without bosses nothing supports the board from below, the staked pegs and the four posts hold it. Assembly order: wires soldered, board onto pegs, stake, then fit the cap. Verify stake strength on a spare board.

## 9. Rebuild in Fusion
- onebutton_build.py builds everything from the parameters at its top. Use a fresh parametric design (Utilities > Scripts > add the script and run, or run it through the Fusion MCP script tool). It creates REF bodies, CASE_SHELL, BOTTOM_CAP, KEYCAP, SHROUD, runs an all-body interference check and prints volumes. Set EXPORT_DIR at the top to also write the four STLs. NEW_DOCUMENT = True builds into a scratch document.
- Verified: the script was run in an empty parametric design and reproduces the model. Expected values (v0.12): CASE_SHELL 6240.1 mm3, BOTTOM_CAP 1673.7, KEYCAP 2138.6, SHROUD 3413.3, 0 interferences. Run it in a design that is already open and empty: creating a new document from inside the script hung Fusion (NEW_DOCUMENT stays False).
- Every cut and join targets one body (participantBodies), so REF bodies are never modified.
- A failed Fusion script rolls back entirely. Editing earlier timeline sketches by moving sketch points and calling computeAll works (used for the hook tip).
