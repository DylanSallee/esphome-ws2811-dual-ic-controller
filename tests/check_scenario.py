#!/usr/bin/env python3
"""Checks the frames printed by tests/host/scenario.yaml. Pass the output file as the argument."""

import sys

GAMMA = 2.8
PIXELS = 14


def parse(path):
    steps, current = {}, None
    for line in open(path, errors="replace"):
        parts = line.split()
        if line.startswith("STEP"):
            current = parts[1]
            steps[current] = []
        elif line.startswith("FRAME") and current is not None:
            steps[current].append([int(value) for value in parts[2:]])
    return steps


def pixel(frame, index=0):
    """Return r, g, b, cold, warm for one pixel. Wire order is R G B W C X."""
    r, g, b, warm, cold, unused = frame[index * 6 : index * 6 + 6]
    assert unused == 0, "the unused byte was driven"
    return r, g, b, cold, warm


def uniform(frame):
    return all(pixel(frame, i) == pixel(frame, 0) for i in range(PIXELS))


def check(steps):
    last = {name: frames[-1] for name, frames in steps.items() if frames}

    def expect(name, test, detail=""):
        status = "ok  " if test else "FAIL"
        print(f"{status} {name} {detail}")
        return bool(test)

    def in_mode(frames, white):
        return [any(pixel(f)[3:]) if white else any(pixel(f)[:3]) for f in frames]

    results = []
    add = lambda *args: results.append(expect(*args))

    add("red is pure red", pixel(last["red_full"]) == (255, 0, 0, 0, 0), pixel(last["red_full"]))
    orange = pixel(last["orange_full"])
    add("orange is decoded, not raw", orange[:3] == (255, round(255 * 0.5**GAMMA), 0), orange)
    add("colours leave the white LEDs off", orange[3:] == (0, 0), orange)
    dim = pixel(last["orange_dim"])
    add("a dim colour is not dim white", dim[0] > 0 and dim[2] == 0, dim)

    cold, mid, warm = (pixel(last[n]) for n in ("white_cold", "white_mid", "white_warm"))
    add("cold white uses only cold LEDs", cold == (0, 0, 0, 255, 0), cold)
    add("warm white uses only warm LEDs", warm == (0, 0, 0, 0, 255), warm)
    add("mid white splits with flat output", mid[:3] == (0, 0, 0) and abs(mid[3] + mid[4] - 255) <= 1, mid)

    fade = steps["crossfade_to_red"]
    sums = [sum(pixel(f)[0:1]) + sum(pixel(f)[3:]) for f in fade]
    add("crossfade has many frames", len(fade) >= 8, f"{len(fade)} frames")
    add("crossfade never dips through black", min(sums) >= 200, f"lowest {min(sums)}")
    add("crossfade mixes both", any(pixel(f)[0] > 50 and pixel(f)[4] > 50 for f in fade))
    add("crossfade ends on target", pixel(fade[-1]) == (255, 0, 0, 0, 0), pixel(fade[-1]))

    add("full white is unlimited at 100%", pixel(last["white_for_limit"]) == (255, 255, 255, 0, 0))
    limited = pixel(last["limit_15"])
    add("limit applies without a new command", limited[:3] == (64, 64, 64), limited)
    limited_red = pixel(last["limit_15_red"])
    add("limit lets a pure colour stay brighter", limited_red[0] == 191, limited_red)
    half = pixel(last["limit_15_half_brightness"])
    expected = round(191 * 0.5**2.2)
    add("slider still works under the limit", abs(half[0] - expected) <= 1, f"{half} expected {expected}")

    down = [pixel(f)[0] for f in steps["fade_off"]]
    add("fade off ends at zero", down[-1] == 0, down[-3:])
    add("fade off never rises", all(a >= b for a, b in zip(down[1:], down[2:])))
    add("fade off keeps the colour", all(pixel(f)[1:] == (0, 0, 0, 0) for f in steps["fade_off"]))

    up = steps["fade_on_new_colour"]
    add("fade on only shows the new colour", all(pixel(f)[0] == 0 for f in up))
    add("fade on reaches full", pixel(up[-1])[2] == 255, pixel(up[-1]))

    jump = steps["interrupted"]
    deltas = [max(abs(a - b) for a, b in zip(pixel(x), pixel(y))) for x, y in zip(jump, jump[1:])]
    add("an interrupted fade does not jump", max(deltas) < 110, f"largest step {max(deltas)}")
    add("an interrupted fade ends on target", pixel(jump[-1]) == (0, 255, 0, 0, 0), pixel(jump[-1]))

    effect = steps["effect_from_white"][2:]
    add("effect turns the white LEDs off", all(pixel(f)[3:] == (0, 0) for f in effect))
    add("effect draws colours", not uniform(effect[-1]))
    add("effect rainbow stays saturated", min(min(pixel(effect[-1], i)[:3]) for i in range(PIXELS)) <= 3)
    add("stopping an effect restores white", pixel(last["effect_stop"]) == (0, 0, 0, 0, 255), pixel(last["effect_stop"]))

    fade_frames = steps["effect_fade_off"]
    lit = [f for f in fade_frames if max(f) >= 8]
    # The fade must never fall back to one solid colour. Below level 8 a rainbow can round to one.
    add("an effect fades out as itself", all(not uniform(f) for f in lit), f"{len(lit)} lit frames")
    add("an effect fade ends black", max(fade_frames[-1]) == 0)
    add("the next colour shows after an effect fade", pixel(last["after_effect_fade"]) == (0, 0, 0, 0, 255))

    held = {tuple(pixel(f, 3)) for f in steps["effect_readback"][3:]}
    add("effects read back what they wrote", len(held) == 1, held)
    full = pixel(last["effect_readback"])
    add("the effect colour is decoded once", full == (129, 19, 3, 0, 0), full)
    half = pixel(last["effect_half_brightness"])
    add("effects follow the brightness slider", half[:2] == (28, 4), half)
    capped = pixel(last["effect_limit_5"])
    load = (129 + 19 + 3) / 255 / 5
    expected = 129 * 0.05 / load
    add("effects stay within the limit", abs(capped[0] - expected) <= 2, f"{capped} expected {expected:.0f}")

    flash = steps["effect_flash"]
    add("flash reaches white", any(pixel(f)[4] > 200 for f in flash))
    add("flash returns to the colour", pixel(flash[-1]) == (255, 0, 0, 0, 0), pixel(flash[-1]))

    warm_mix = pixel(last["white_mix_warm"])
    add("a warm white colour uses the warm LEDs", warm_mix[4] == 255 and warm_mix[2] == 0, warm_mix)
    add("a saturated colour stays off the white LEDs", pixel(last["white_mix_red"]) == (255, 0, 0, 0, 0))
    frames = steps["balance_live"]
    add("balance applies to the next frame", any(pixel(f)[:2] == (255, 128) for f in frames), pixel(frames[0]))
    add("balance can change without a new command", pixel(frames[-1])[:2] == (255, 255), pixel(frames[-1]))

    add("an idle strip is sent nothing", not steps.get("idle"), f"{len(steps.get('idle', []))} frames")
    add("the scenario finished", "done" in steps)
    return all(results)


if __name__ == "__main__":
    sys.exit(0 if check(parse(sys.argv[1])) else 1)
