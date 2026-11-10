#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace rts {

// Metres, seconds and radians. The site origin is NOT the runway centre.
// Inspired by SupCom's Winged/MinAirspeed/TurnSpeed/BankFactor separation;
// these are our own metre-scale approach rules, not its engine or game assets.
inline constexpr float kAirfieldRunwayOffsetX = -55.0f;

class AirfieldApproach {
    struct Point { double x = 0.0, y = 0.0; };
    std::array<Point, 6> controls_{};
    std::array<double, 257> lengths_{};
    double distance_ = 0.0;
    double roll_time_ = 0.0;
    double sign_ = 1.0;
    double runway_x_ = 0.0, runway_y_ = 0.0;
    double cruise_ = 100.0;
    static constexpr double kFinalLength = 1580.0; // fix at 1800, touchdown at 220
    static constexpr double kRollLength = 220.0;
    static constexpr double kLandingSpeed = 70.0;

    template<size_t N>
    static Point evaluate(std::array<Point, N> points, double t) {
        for (size_t n=N-1; n>0; --n)
            for (size_t i=0; i<n; ++i)
                points[i] = {points[i].x*(1-t)+points[i+1].x*t,
                             points[i].y*(1-t)+points[i+1].y*t};
        return points[0];
    }
    Point curve(double t) const { return evaluate(controls_, t); }

public:
    struct Pose {
        float x = 0, y = 0, altitude = 160, heading = 0, pitch = 0, bank = 0, speed = 0;
        bool finished = false;
    } pose;

    AirfieldApproach() = default;
    AirfieldApproach(float x, float y, float heading, float ax, float ay, float speed) {
        runway_x_ = ax + kAirfieldRunwayOffsetX;
        runway_y_ = ay;
        sign_ = y >= ay ? 1.0 : -1.0;
        cruise_ = std::clamp(double(speed), kLandingSpeed, 120.0);
        const Point fix{runway_x_, runway_y_ + sign_ * 1800.0};
        const double handle = std::clamp(std::hypot(fix.x-x, fix.y-y) * 0.28, 1000.0, 4000.0);
        // Quintic entry has zero curvature at both ends: no bank snap where
        // cruise joins the turn or where the turn joins the straight final.
        controls_ = {{{x,y},
            {x + std::sin(heading)*handle, y + std::cos(heading)*handle},
            {x + std::sin(heading)*handle*2, y + std::cos(heading)*handle*2},
            {fix.x,fix.y+sign_*2000}, {fix.x,fix.y+sign_*1000}, fix}};
        Point before = controls_[0];
        for (size_t i = 1; i < lengths_.size(); ++i) {
            const Point point = curve(double(i)/(lengths_.size()-1));
            lengths_[i] = lengths_[i-1] + std::hypot(point.x-before.x, point.y-before.y);
            before = point;
        }
        pose = {x, y, 160.0f, heading, 0, 0, float(cruise_), false};
    }

    void advance(float seconds) {
        if (pose.finished) return;
        const double curve_length = lengths_.back();
        const double touchdown = curve_length + kFinalLength;
        double speed = cruise_;
        if (distance_ > curve_length) {
            const double fraction = std::clamp((distance_-curve_length)/kFinalLength, 0.0, 1.0);
            speed = cruise_ + (kLandingSpeed-cruise_)*fraction;
        }
        if (distance_ < touchdown) {
            distance_ = std::min(touchdown, distance_ + speed*seconds);
        } else {
            // Constant braking on the ground: finite forward motion until rest.
            roll_time_ = std::min(2.0*kRollLength/kLandingSpeed, roll_time_+seconds);
            const double brake = kLandingSpeed*kLandingSpeed/(2.0*kRollLength);
            distance_ = touchdown + kLandingSpeed*roll_time_ - 0.5*brake*roll_time_*roll_time_;
            speed = std::max(0.0, kLandingSpeed-brake*roll_time_);
        }
        Point point;
        double heading = -sign_ > 0 ? 0.0 : std::acos(-1.0);
        double altitude = 160.0, pitch = 0.0, bank = 0.0;
        if (distance_ < curve_length) {
            auto it = std::upper_bound(lengths_.begin(), lengths_.end(), distance_);
            const size_t i = std::clamp<size_t>(it-lengths_.begin(), 1, lengths_.size()-1);
            const double t = (i-1 + (distance_-lengths_[i-1])/(lengths_[i]-lengths_[i-1]))/(lengths_.size()-1);
            point = curve(t);
            std::array<Point,5> first{};
            std::array<Point,4> second{};
            for (size_t j=0;j<first.size();++j)
                first[j] = {(controls_[j+1].x-controls_[j].x)*5, (controls_[j+1].y-controls_[j].y)*5};
            for (size_t j=0;j<second.size();++j)
                second[j] = {(first[j+1].x-first[j].x)*4, (first[j+1].y-first[j].y)*4};
            const auto derivative = evaluate(first,t);
            const auto acceleration = evaluate(second,t);
            const double dx=derivative.x, dy=derivative.y, ddx=acceleration.x, ddy=acceleration.y;
            const double curvature = (dx*ddy-dy*ddx)/std::pow(std::hypot(dx,dy),3);
            heading = std::atan2(dx,dy);
            bank = std::clamp(std::atan(speed*speed*curvature/9.81), -0.45, 0.45);
        } else {
            const double final_distance = distance_-curve_length;
            point = {runway_x_, runway_y_+sign_*(1800.0-final_distance)};
            const double t = std::clamp(final_distance/kFinalLength, 0.0, 1.0);
            // Smooth flare, no vertical drop after stopping. The descent is
            // tied to forward travel, never to render-frame wall time.
            altitude = 160.0*(1.0-t*t*(3.0-2.0*t));
            pitch = std::atan(-160.0*6.0*t*(1.0-t)/kFinalLength);
        }
        const bool finished = roll_time_ >= 2.0*kRollLength/kLandingSpeed;
        pose = {float(point.x), float(point.y), float(altitude), float(heading), float(pitch), float(bank), float(speed), finished};
    }
};
} // namespace rts
