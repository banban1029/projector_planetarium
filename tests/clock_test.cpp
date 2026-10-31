// シミュレーション時刻・季節ジャンプ・シナリオ再生のテスト
#include <cmath>
#include <cstdio>
#include <string>

#include "scenario.hpp"
#include "seasons.hpp"
#include "sim_clock.hpp"

namespace {
int failures = 0;

void check(const char* name, bool ok, const std::string& detail = "") {
    std::printf("[%s] %-45s %s\n", ok ? " OK " : "FAIL", name, detail.c_str());
    if (!ok) ++failures;
}

void checkLocal(const char* name, const SimClock& c, const char* expected) {
    const std::string actual = c.formatLocal().substr(0, 16);  // "YYYY-MM-DD HH:MM"
    check(name, actual == expected, actual + " (expected " + expected + ")");
}
}  // namespace

int main() {
    const std::vector<double> steps{1, 10, 60, 3600, 86400};

    // 地方時の文字列 <-> 内部時刻
    {
        SimClock c(9.0, steps, 1.0);
        check("parse local time", c.setLocalString("2026-12-15 22:00"));
        checkLocal("format local time", c, "2026-12-15 22:00");
        check("JST 2000-01-01 21:00 = J2000.0",
              (c.setLocalString("2000-01-01 21:00"), std::abs(c.julianDate() - 2451545.0) < 1e-9));
        check("reject bad time string", !c.setLocalString("next tuesday"));
    }

    // 倍率・逆再生・一時停止
    {
        SimClock c(9.0, steps, 60.0);
        c.setLocalString("2026-01-01 00:00");
        c.update(10.0);
        checkLocal("x60 for 10 s = +10 min", c, "2026-01-01 00:10");
        c.faster();
        c.update(1.0);
        checkLocal("faster -> x3600, 1 s = +1 h", c, "2026-01-01 01:10");
        c.toggleReverse();
        c.update(2.0);
        checkLocal("reverse 2 s = -2 h", c, "2025-12-31 23:10");
        c.togglePause();
        c.update(100.0);
        checkLocal("paused", c, "2025-12-31 23:10");
        c.faster();
        c.faster();  // 上限を超えない
        check("speed clamps at max step", c.speed() == 86400.0);
        c.resetRate();
        check("resetRate -> x1, forward, playing", c.speed() == 1.0 && !c.reversed() && !c.paused());
    }

    // 季節の巡回（年をまたぐ）
    {
        SimClock c(9.0, steps, 1.0);
        SeasonCycler s({{"春", 4, 15, 21, 0}, {"夏", 8, 10, 21, 0}, {"秋", 10, 20, 21, 0}, {"冬", 1, 20, 21, 0}},
                       30.0, false);
        c.setLocalString("2026-10-31 21:00");
        s.next(c);
        checkLocal("next from Oct 31 -> winter next year", c, "2027-01-20 21:00");
        check("current season name", s.currentName() == "冬");
        s.next(c);
        checkLocal("next -> spring", c, "2027-04-15 21:00");
        s.prev(c);
        s.prev(c);
        checkLocal("prev x2 -> autumn 2026", c, "2026-10-20 21:00");
        s.jumpTo(1, c);
        checkLocal("jumpTo summer -> nearest", c, "2026-08-10 21:00");

        s.toggleTour();
        s.update(29.0, c);
        checkLocal("tour waits for interval", c, "2026-08-10 21:00");
        s.update(1.0, c);
        checkLocal("tour advances after 30 s", c, "2026-10-20 21:00");
    }

    // シナリオ
    {
        Scenario sc;
        sc.loop = false;
        sc.steps = {{5.0, "achi", "", -1, "", "A", -1, -1}, {3.0, "", "", 600, "", "B", -1, -1}};
        ScenarioPlayer p(sc);
        const ScenarioStep* st = p.start();
        check("scenario starts at first step", st && st->message == "A" && p.playing());
        check("no change before duration", p.update(4.9) == nullptr);
        st = p.update(0.2);
        check("second step after 5 s", st && st->message == "B" && st->speed == 600);
        check("stops at the end without loop", p.update(3.0) == nullptr && !p.playing());

        sc.loop = true;
        ScenarioPlayer q(sc);
        q.start();
        q.update(5.0);
        st = q.update(3.0);
        check("loops back to first step", st && st->message == "A" && q.playing());
    }

    std::printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
