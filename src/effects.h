//
// Visual effects toolbox: particles, shockwave rings, screen shake, flash,
// vignette, CRT scanlines, digital rain and glitch bars.
//
// Everything is drawn with the plain SDL renderer and uses the default BLEND
// blend mode, so it stays correct on the transparent (desktop overlay) window.
//

#ifndef EFFECTS_H
#define EFFECTS_H

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace fx {
    constexpr float PI = 3.14159265358979f;

    inline float rnd() { return SDL_randf(); }
    inline float rnd(const float a, const float b) { return a + (b - a) * SDL_randf(); }
    inline float clamp01(const float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

    inline float smoothstep(const float a, const float b, const float v) {
        const float t = clamp01((v - a) / (b - a));
        return t * t * (3.f - 2.f * t);
    }

    inline Uint8 to_u8(const float v) { return static_cast<Uint8>(clamp01(v) * 255.f + 0.5f); }

    inline SDL_Color rgb(const int r, const int g, const int b) {
        return SDL_Color{static_cast<Uint8>(r), static_cast<Uint8>(g), static_cast<Uint8>(b), 255};
    }

    inline SDL_Color lerp(const SDL_Color &a, const SDL_Color &b, const float t) {
        auto mix = [t](const Uint8 x, const Uint8 y) {
            return static_cast<Uint8>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
        };
        return SDL_Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), 255};
    }

    // h in [0, 1) (wraps), s and v in [0, 1]
    inline SDL_Color hsv(float h, const float s, const float v) {
        h -= std::floor(h);
        const float h6 = h * 6.f;
        const int i = static_cast<int>(h6);
        const float f = h6 - static_cast<float>(i);
        const float p = v * (1.f - s);
        const float q = v * (1.f - f * s);
        const float t = v * (1.f - (1.f - f) * s);
        float r, g, b;
        switch (i % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
        return SDL_Color{to_u8(r), to_u8(g), to_u8(b), 255};
    }

    enum class Shape { Glow, Square, Spark };

    struct Particle {
        float x = 0, y = 0, vx = 0, vy = 0;
        float life = 0, max_life = 1;
        float size0 = 8, size1 = 0;
        float rot = 0, vrot = 0;
        float drag = 0, gravity = 0;
        float alpha = 1;
        SDL_Color c0{255, 255, 255, 255};
        SDL_Color c1{255, 255, 255, 255};
        Shape shape = Shape::Glow;
    };

    struct Ring {
        float x = 0, y = 0, r0 = 0, r1 = 100;
        float life = 0, max_life = 1;
        float alpha = 1;
        SDL_Color color{255, 255, 255, 255};
    };

    struct Burst {
        int count = 30;
        float speed_min = 100, speed_max = 400;
        float angle_min = 0, angle_max = 2.f * PI;
        float life_min = 0.5f, life_max = 1.2f;
        float size_min = 6, size_max = 14;
        float size_end = 0; // final size as a multiple of the start size
        float drag = 1.5f;
        float gravity = 0;
        float spin = 0; // max rotation speed, rad/s
        float radius = 0; // spawn radius
        float alpha = 1;
        SDL_Color c0{255, 255, 255, 255};
        SDL_Color c1{255, 255, 255, 255};
        Shape shape = Shape::Glow;
        bool rainbow = false; // ignore c0/c1, use random hues
    };

    class Effects {
    public:
        bool init(SDL_Renderer *renderer, const int width, const int height) {
            width_ = width;
            height_ = height;
            glow_ = make_texture(64, 64, [](const float dx, const float dy) {
                const float d = std::sqrt(dx * dx + dy * dy);
                const float a = clamp01(1.f - d);
                return a * a;
            }, renderer);
            ring_ = make_texture(256, 256, [](const float dx, const float dy) {
                const float d = std::sqrt(dx * dx + dy * dy);
                const float k = (d - 0.9f) / 0.035f;
                const float a = std::exp(-k * k) + 0.03f * clamp01(d) * clamp01(d) * clamp01(d) * clamp01(d);
                return d > 1.f ? 0.f : clamp01(a);
            }, renderer);
            px_ = make_texture(8, 8, [](float, float) { return 1.f; }, renderer);
            if (glow_ == nullptr || ring_ == nullptr || px_ == nullptr) {
                return false;
            }

            // digital rain
            const int columns = std::max(8, width / 16);
            drops_.resize(static_cast<size_t>(columns));
            for (int i = 0; i < columns; i++) {
                Drop &d = drops_[static_cast<size_t>(i)];
                d.x = (static_cast<float>(i) + rnd(0.1f, 0.9f)) * static_cast<float>(width) / static_cast<float>(columns);
                reset_drop(d, true);
            }

            // scanlines
            for (int y = 0; y < height; y += 3) {
                scanlines_.push_back(SDL_FRect{0.f, static_cast<float>(y), static_cast<float>(width), 1.f});
            }
            return true;
        }

        void destroy() {
            if (glow_) SDL_DestroyTexture(glow_);
            if (ring_) SDL_DestroyTexture(ring_);
            if (px_) SDL_DestroyTexture(px_);
            glow_ = ring_ = px_ = nullptr;
        }

        // ---------------------------------------------------------------- update

        void update(const float dt) {
            time_ += dt;

            for (Particle &p: particles_) {
                p.life -= dt;
                const float keep = std::exp(-p.drag * dt);
                p.vx *= keep;
                p.vy *= keep;
                p.vy += p.gravity * dt;
                p.x += p.vx * dt;
                p.y += p.vy * dt;
                p.rot += p.vrot * dt;
            }
            particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                            [](const Particle &p) { return p.life <= 0.f; }), particles_.end());

            for (Ring &r: rings_) {
                r.life -= dt;
            }
            rings_.erase(std::remove_if(rings_.begin(), rings_.end(),
                                        [](const Ring &r) { return r.life <= 0.f; }), rings_.end());

            trauma_ = std::max(0.f, trauma_ - 1.5f * dt);
            flash_ = std::max(0.f, flash_ - 2.2f * dt);
            glitch_ = std::max(0.f, glitch_ - dt);

            const float mag = 34.f * trauma_ * trauma_;
            shake_x_ = rnd(-1.f, 1.f) * mag;
            shake_y_ = rnd(-1.f, 1.f) * mag;

            for (Drop &d: drops_) {
                d.y += d.speed * dt;
                if (d.y - d.len > static_cast<float>(height_)) {
                    reset_drop(d, false);
                }
            }
        }

        // ---------------------------------------------------------------- emitters

        void burst(const float x, const float y, const Burst &b) {
            for (int i = 0; i < b.count; i++) {
                if (particles_.size() >= MAX_PARTICLES) {
                    return;
                }
                Particle p;
                const float angle = rnd(b.angle_min, b.angle_max);
                const float speed = rnd(b.speed_min, b.speed_max);
                const float spawn_angle = rnd(0.f, 2.f * PI);
                const float spawn_r = b.radius * std::sqrt(rnd());
                p.x = x + std::cos(spawn_angle) * spawn_r;
                p.y = y + std::sin(spawn_angle) * spawn_r;
                p.vx = std::cos(angle) * speed;
                p.vy = std::sin(angle) * speed;
                p.max_life = rnd(b.life_min, b.life_max);
                p.life = p.max_life;
                p.size0 = rnd(b.size_min, b.size_max);
                p.size1 = p.size0 * b.size_end;
                p.rot = rnd(0.f, 2.f * PI);
                p.vrot = rnd(-b.spin, b.spin);
                p.drag = b.drag;
                p.gravity = b.gravity;
                p.alpha = b.alpha;
                p.shape = b.shape;
                if (b.rainbow) {
                    p.c0 = hsv(rnd(), 0.75f, 1.f);
                    p.c1 = p.c0;
                } else {
                    p.c0 = b.c0;
                    p.c1 = b.c1;
                }
                particles_.push_back(p);
            }
        }

        void ring(const float x, const float y, const float r0, const float r1, const float duration,
                  const SDL_Color &color, const float alpha = 1.f) {
            Ring r;
            r.x = x;
            r.y = y;
            r.r0 = r0;
            r.r1 = r1;
            r.max_life = duration;
            r.life = duration;
            r.color = color;
            r.alpha = alpha;
            rings_.push_back(r);
        }

        // Dense burst of sparks for small impacts (clicks, bounces, countdown ticks)
        void sparks(const float x, const float y, const SDL_Color &c0, const SDL_Color &c1, const int count = 18,
                    const float speed = 450.f) {
            Burst b;
            b.count = count;
            b.speed_min = speed * 0.3f;
            b.speed_max = speed;
            b.life_min = 0.25f;
            b.life_max = 0.7f;
            b.size_min = 6;
            b.size_max = 12;
            b.drag = 2.5f;
            b.shape = Shape::Spark;
            b.c0 = c0;
            b.c1 = c1;
            burst(x, y, b);
            b.shape = Shape::Glow;
            b.count = count / 2;
            b.size_min = 10;
            b.size_max = 22;
            b.alpha = 0.6f;
            burst(x, y, b);
        }

        void confetti(const float x, const float y, const int count = 120) {
            Burst b;
            b.count = count;
            b.speed_min = 200;
            b.speed_max = 1100;
            b.life_min = 1.0f;
            b.life_max = 2.2f;
            b.size_min = 8;
            b.size_max = 16;
            b.size_end = 0.6f;
            b.drag = 1.2f;
            b.gravity = 700;
            b.spin = 14;
            b.shape = Shape::Square;
            b.rainbow = true;
            burst(x, y, b);
            b.shape = Shape::Glow;
            b.count = count / 3;
            b.size_min = 14;
            b.size_max = 34;
            b.gravity = 0;
            b.alpha = 0.55f;
            burst(x, y, b);
            ring(x, y, 10, 520, 0.9f, rgb(255, 255, 255), 0.9f);
            ring(x, y, 10, 360, 0.7f, hsv(rnd(), 0.8f, 1.f), 0.9f);
        }

        void explosion(const float x, const float y) {
            flash(1.0f);
            add_shake(1.0f);

            ring(x, y, 20, 1500, 1.35f, rgb(255, 255, 255), 0.35f);
            ring(x, y, 20, 1000, 1.0f, rgb(255, 170, 40), 0.9f);
            ring(x, y, 20, 640, 0.7f, rgb(255, 240, 160), 0.9f);

            Burst fire;
            fire.count = 34;
            fire.speed_min = 40;
            fire.speed_max = 420;
            fire.life_min = 0.5f;
            fire.life_max = 1.0f;
            fire.size_min = 70;
            fire.size_max = 130;
            fire.size_end = 0.3f;
            fire.drag = 2.f;
            fire.radius = 60;
            fire.alpha = 0.85f;
            fire.c0 = rgb(255, 230, 120);
            fire.c1 = rgb(230, 40, 0);
            burst(x, y, fire);

            Burst sparks_b;
            sparks_b.count = 170;
            sparks_b.speed_min = 300;
            sparks_b.speed_max = 1600;
            sparks_b.life_min = 0.5f;
            sparks_b.life_max = 1.5f;
            sparks_b.size_min = 8;
            sparks_b.size_max = 16;
            sparks_b.drag = 1.8f;
            sparks_b.gravity = 450;
            sparks_b.shape = Shape::Spark;
            sparks_b.c0 = rgb(255, 245, 190);
            sparks_b.c1 = rgb(255, 80, 10);
            burst(x, y, sparks_b);

            Burst debris;
            debris.count = 70;
            debris.speed_min = 200;
            debris.speed_max = 950;
            debris.life_min = 1.0f;
            debris.life_max = 2.0f;
            debris.size_min = 6;
            debris.size_max = 15;
            debris.size_end = 0.5f;
            debris.drag = 0.8f;
            debris.gravity = 1000;
            debris.spin = 16;
            debris.shape = Shape::Square;
            debris.c0 = rgb(255, 150, 30);
            debris.c1 = rgb(40, 20, 20);
            burst(x, y, debris);

            Burst smoke;
            smoke.count = 36;
            smoke.speed_min = 30;
            smoke.speed_max = 320;
            smoke.life_min = 1.2f;
            smoke.life_max = 2.4f;
            smoke.size_min = 50;
            smoke.size_max = 100;
            smoke.size_end = 2.2f;
            smoke.drag = 1.6f;
            smoke.gravity = -60;
            smoke.alpha = 0.35f;
            smoke.radius = 120;
            smoke.c0 = rgb(110, 70, 60);
            smoke.c1 = rgb(10, 10, 10);
            burst(x, y, smoke);
        }

        // ---------------------------------------------------------------- screen-wide state

        void add_shake(const float trauma) { trauma_ = std::min(1.f, trauma_ + trauma); }
        void keep_shake(const float level) { trauma_ = std::max(trauma_, std::min(1.f, level)); }

        void flash(const float strength, const SDL_Color &color = SDL_Color{255, 255, 255, 255}) {
            flash_ = std::max(flash_, strength);
            flash_color_ = color;
        }

        void trigger_glitch(const float duration) { glitch_ = std::max(glitch_, duration); }
        bool glitching() const { return glitch_ > 0.f; }
        float time() const { return time_; }
        size_t particle_count() const { return particles_.size() + rings_.size(); }

        // Shakes everything drawn between begin_shake() and end_shake()
        void begin_shake(SDL_Renderer *renderer) const {
            if (trauma_ <= 0.001f) {
                return;
            }
            const SDL_Rect viewport = {static_cast<int>(shake_x_), static_cast<int>(shake_y_), width_, height_};
            SDL_SetRenderViewport(renderer, &viewport);
        }

        static void end_shake(SDL_Renderer *renderer) { SDL_SetRenderViewport(renderer, nullptr); }

        // ---------------------------------------------------------------- drawing

        // Draws rings and particles
        void draw(SDL_Renderer *renderer) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

            for (const Ring &r: rings_) {
                const float t = r.life / r.max_life; // 1 -> 0
                const float k = 1.f - t;
                const float ease = 1.f - (1.f - k) * (1.f - k) * (1.f - k);
                const float radius = r.r0 + (r.r1 - r.r0) * ease;
                SDL_SetTextureColorMod(ring_, r.color.r, r.color.g, r.color.b);
                SDL_SetTextureAlphaMod(ring_, to_u8(r.alpha * t * t));
                const SDL_FRect dst = {r.x - radius, r.y - radius, radius * 2.f, radius * 2.f};
                SDL_RenderTexture(renderer, ring_, nullptr, &dst);
            }

            for (const Particle &p: particles_) {
                const float t = p.life / p.max_life;
                const float k = 1.f - t;
                const float size = p.size0 + (p.size1 - p.size0) * k;
                const SDL_Color c = lerp(p.c0, p.c1, k);
                const float a = p.alpha * smoothstep(0.f, 0.35f, t);
                switch (p.shape) {
                    case Shape::Glow: {
                        SDL_SetTextureColorMod(glow_, c.r, c.g, c.b);
                        SDL_SetTextureAlphaMod(glow_, to_u8(a));
                        const SDL_FRect dst = {p.x - size, p.y - size, size * 2.f, size * 2.f};
                        SDL_RenderTexture(renderer, glow_, nullptr, &dst);
                        break;
                    }
                    case Shape::Square: {
                        SDL_SetTextureColorMod(px_, c.r, c.g, c.b);
                        SDL_SetTextureAlphaMod(px_, to_u8(a));
                        const SDL_FRect dst = {p.x - size * 0.5f, p.y - size * 0.5f, size, size * 0.6f};
                        SDL_RenderTextureRotated(renderer, px_, nullptr, &dst, static_cast<double>(p.rot * 180.f / PI),
                                                 nullptr, SDL_FLIP_NONE);
                        break;
                    }
                    case Shape::Spark: {
                        const float speed = std::sqrt(p.vx * p.vx + p.vy * p.vy);
                        if (speed < 1.f) {
                            break;
                        }
                        const float dx = p.vx / speed;
                        const float dy = p.vy / speed;
                        const float len = std::clamp(speed * 0.06f, 5.f, 80.f) * (0.4f + 0.6f * t);
                        const float half = 0.6f + size * 0.12f;
                        draw_gradient_quad(renderer,
                                           p.x - dx * len - dy * half, p.y - dy * len + dx * half,
                                           p.x - dx * len + dy * half, p.y - dy * len - dx * half,
                                           p.x + dy * half, p.y - dx * half,
                                           p.x - dy * half, p.y + dx * half,
                                           c, 0.f, a);
                        break;
                    }
                }
            }
            SDL_SetTextureColorMod(glow_, 255, 255, 255);
            SDL_SetTextureAlphaMod(glow_, 255);
        }

        // Soft round light
        void draw_glow(SDL_Renderer *renderer, const float x, const float y, const float radius,
                       const SDL_Color &color, const float alpha) const {
            SDL_SetTextureColorMod(glow_, color.r, color.g, color.b);
            SDL_SetTextureAlphaMod(glow_, to_u8(alpha));
            const SDL_FRect dst = {x - radius, y - radius, radius * 2.f, radius * 2.f};
            SDL_RenderTexture(renderer, glow_, nullptr, &dst);
            SDL_SetTextureColorMod(glow_, 255, 255, 255);
            SDL_SetTextureAlphaMod(glow_, 255);
        }

        // Filled rectangle (alpha-blended)
        static void fill_rect(SDL_Renderer *renderer, const SDL_FRect &rect, const SDL_Color &color, const float alpha) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, to_u8(alpha));
            SDL_RenderFillRect(renderer, &rect);
        }

        // Darkens/tints the edges of the screen
        void draw_vignette(SDL_Renderer *renderer, const SDL_Color &color, const float strength,
                           const float size = 260.f) const {
            if (strength <= 0.01f) {
                return;
            }
            const float w = static_cast<float>(width_);
            const float h = static_cast<float>(height_);
            // top, bottom, left, right
            draw_gradient_quad(renderer, 0, 0, w, 0, w, size, 0, size, color, strength, 0.f);
            draw_gradient_quad(renderer, 0, h, w, h, w, h - size, 0, h - size, color, strength, 0.f);
            draw_gradient_quad(renderer, 0, 0, 0, h, size, h, size, 0, color, strength, 0.f);
            draw_gradient_quad(renderer, w, 0, w, h, w - size, h, w - size, 0, color, strength, 0.f);
        }

        void draw_flash(SDL_Renderer *renderer) const {
            if (flash_ <= 0.01f) {
                return;
            }
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, flash_color_.r, flash_color_.g, flash_color_.b, to_u8(flash_));
            SDL_RenderFillRect(renderer, nullptr);
        }

        // CRT look: thin dark lines and a slow bright band rolling down the screen
        void draw_scanlines(SDL_Renderer *renderer, const SDL_Color &band_color, const float line_alpha = 0.16f,
                            const float band_alpha = 0.07f) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, to_u8(line_alpha));
            SDL_RenderFillRects(renderer, scanlines_.data(), static_cast<int>(scanlines_.size()));

            const float h = static_cast<float>(height_);
            const float w = static_cast<float>(width_);
            const float band_h = 220.f;
            const float y = std::fmod(time_ * 140.f, h + band_h * 2.f) - band_h * 2.f;
            draw_gradient_quad(renderer, 0, y, w, y, w, y + band_h, 0, y + band_h, band_color, 0.f, band_alpha);
            draw_gradient_quad(renderer, 0, y + band_h, w, y + band_h, w, y + band_h * 2.f, 0, y + band_h * 2.f,
                               band_color, band_alpha, 0.f);
        }

        // Falling streaks of light
        void draw_rain(SDL_Renderer *renderer, const SDL_Color &color, const float intensity) {
            if (intensity <= 0.01f) {
                return;
            }
            const SDL_Color head_color = lerp(color, rgb(255, 255, 255), 0.6f);
            for (const Drop &d: drops_) {
                const float half = d.width * 0.5f;
                draw_gradient_quad(renderer,
                                   d.x - half, d.y - d.len, d.x + half, d.y - d.len,
                                   d.x + half, d.y, d.x - half, d.y,
                                   color, 0.f, d.alpha * intensity);
                draw_glow(renderer, d.x, d.y, 5.f + d.width * 2.f, head_color, d.alpha * intensity * 0.9f);
            }
        }

        // Horizontal tearing bars, only while a glitch is active
        void draw_glitch_bars(SDL_Renderer *renderer, const SDL_Color &color) const {
            if (!glitching()) {
                return;
            }
            const int bars = 3 + static_cast<int>(rnd(0.f, 4.f));
            for (int i = 0; i < bars; i++) {
                const SDL_FRect bar = {
                    0.f, rnd(0.f, static_cast<float>(height_)), static_cast<float>(width_), rnd(2.f, 14.f)
                };
                fill_rect(renderer, bar, i % 2 == 0 ? color : rgb(255, 255, 255), rnd(0.05f, 0.22f));
            }
        }

        // Quad where the first edge (p0-p1) has alpha a0 and the opposite edge (p2-p3) has alpha a1
        static void draw_gradient_quad(SDL_Renderer *renderer,
                                       const float x0, const float y0, const float x1, const float y1,
                                       const float x2, const float y2, const float x3, const float y3,
                                       const SDL_Color &color, const float a0, const float a1) {
            const float r = static_cast<float>(color.r) / 255.f;
            const float g = static_cast<float>(color.g) / 255.f;
            const float b = static_cast<float>(color.b) / 255.f;
            SDL_Vertex v[4] = {
                {{x0, y0}, {r, g, b, a0}, {0.f, 0.f}},
                {{x1, y1}, {r, g, b, a0}, {0.f, 0.f}},
                {{x2, y2}, {r, g, b, a1}, {0.f, 0.f}},
                {{x3, y3}, {r, g, b, a1}, {0.f, 0.f}},
            };
            const int indices[6] = {0, 1, 2, 2, 3, 0};
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_RenderGeometry(renderer, nullptr, v, 4, indices, 6);
        }

    private:
        struct Drop {
            float x = 0, y = 0, speed = 300, len = 120, width = 2, alpha = 0.5f;
        };

        static constexpr size_t MAX_PARTICLES = 3000;

        template<typename F>
        static SDL_Texture *make_texture(const int w, const int h, F alpha_at, SDL_Renderer *renderer) {
            SDL_Surface *surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
            if (surface == nullptr) {
                return nullptr;
            }
            for (int y = 0; y < h; y++) {
                Uint8 *row = static_cast<Uint8 *>(surface->pixels) + y * surface->pitch;
                for (int x = 0; x < w; x++) {
                    const float dx = (static_cast<float>(x) + 0.5f) / static_cast<float>(w) * 2.f - 1.f;
                    const float dy = (static_cast<float>(y) + 0.5f) / static_cast<float>(h) * 2.f - 1.f;
                    row[x * 4 + 0] = 255;
                    row[x * 4 + 1] = 255;
                    row[x * 4 + 2] = 255;
                    row[x * 4 + 3] = to_u8(alpha_at(dx, dy));
                }
            }
            SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_DestroySurface(surface);
            if (texture != nullptr) {
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
            }
            return texture;
        }

        void reset_drop(Drop &d, const bool anywhere) const {
            d.speed = rnd(280.f, 760.f);
            d.len = rnd(80.f, 320.f);
            d.width = rnd(1.5f, 3.f);
            d.alpha = rnd(0.25f, 0.85f);
            d.y = anywhere ? rnd(0.f, static_cast<float>(height_)) : -rnd(0.f, 400.f);
        }

        SDL_Texture *glow_ = nullptr;
        SDL_Texture *ring_ = nullptr;
        SDL_Texture *px_ = nullptr;
        int width_ = 0;
        int height_ = 0;
        float time_ = 0;
        float trauma_ = 0;
        float shake_x_ = 0;
        float shake_y_ = 0;
        float flash_ = 0;
        SDL_Color flash_color_{255, 255, 255, 255};
        float glitch_ = 0;
        std::vector<Particle> particles_;
        std::vector<Ring> rings_;
        std::vector<Drop> drops_;
        std::vector<SDL_FRect> scanlines_;
    };
}

#endif //EFFECTS_H
