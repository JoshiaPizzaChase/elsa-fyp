import React, {useRef, useEffect, useCallback} from 'react';

const STAR_COUNT = 200;
const TRAIL_LENGTH = 6;
const MOUSE_RADIUS = 120;

function StarField() {
    const canvasRef = useRef(null);
    const starsRef = useRef([]);
    const mouseRef = useRef({x: -9999, y: -9999});
    const rafRef = useRef(null);

    const initStars = useCallback((width, height) => {
        const stars = [];
        for (let i = 0; i < STAR_COUNT; i++) {
            stars.push({
                x: Math.random() * width,
                y: Math.random() * height,
                baseRadius: Math.random() * 1.8 + 0.4,
                speed: Math.random() * 0.3 + 0.05,
                drift: (Math.random() - 0.5) * 0.15,
                opacity: Math.random() * 0.6 + 0.4,
                twinkleSpeed: Math.random() * 0.02 + 0.005,
                twinklePhase: Math.random() * Math.PI * 2,
                trail: [],
            });
        }
        starsRef.current = stars;
    }, []);

    useEffect(() => {
        const canvas = canvasRef.current;
        const ctx = canvas.getContext('2d');
        let width = (canvas.width = window.innerWidth);
        let height = (canvas.height = window.innerHeight);

        initStars(width, height);

        const handleResize = () => {
            width = canvas.width = window.innerWidth;
            height = canvas.height = window.innerHeight;
        };

        const handleMouseMove = (e) => {
            mouseRef.current = {x: e.clientX, y: e.clientY};
        };

        const handleMouseLeave = () => {
            mouseRef.current = {x: -9999, y: -9999};
        };

        window.addEventListener('resize', handleResize);
        window.addEventListener('mousemove', handleMouseMove);
        window.addEventListener('mouseleave', handleMouseLeave);

        const draw = () => {
            ctx.clearRect(0, 0, width, height);
            const mouse = mouseRef.current;
            const time = performance.now() * 0.001;

            for (const star of starsRef.current) {
                // Move star
                star.y -= star.speed;
                star.x += star.drift;

                // Wrap around
                if (star.y < -10) {
                    star.y = height + 10;
                    star.x = Math.random() * width;
                }
                if (star.x < -10) star.x = width + 10;
                if (star.x > width + 10) star.x = -10;

                // Mouse repulsion / attraction glow
                const dx = star.x - mouse.x;
                const dy = star.y - mouse.y;
                const dist = Math.sqrt(dx * dx + dy * dy);
                let interactionScale = 1;
                if (dist < MOUSE_RADIUS) {
                    const force = (MOUSE_RADIUS - dist) / MOUSE_RADIUS;
                    const angle = Math.atan2(dy, dx);
                    star.x += Math.cos(angle) * force * 1.5;
                    star.y += Math.sin(angle) * force * 1.5;
                    interactionScale = 1 + force * 2;
                }

                // Record trail
                star.trail.push({x: star.x, y: star.y});
                if (star.trail.length > TRAIL_LENGTH) {
                    star.trail.shift();
                }

                // Twinkle
                const twinkle = 0.5 + 0.5 * Math.sin(time * star.twinkleSpeed * 60 + star.twinklePhase);
                const currentOpacity = star.opacity * (0.6 + 0.4 * twinkle);
                const currentRadius = star.baseRadius * interactionScale;

                // Draw trail
                if (star.trail.length > 1) {
                    for (let i = 0; i < star.trail.length - 1; i++) {
                        const t = star.trail[i];
                        const trailOpacity = currentOpacity * (i / star.trail.length) * 0.3;
                        const trailRadius = currentRadius * (i / star.trail.length) * 0.5;
                        ctx.beginPath();
                        ctx.arc(t.x, t.y, trailRadius, 0, Math.PI * 2);
                        ctx.fillStyle = `rgba(180, 180, 255, ${trailOpacity})`;
                        ctx.fill();
                    }
                }

                // Draw star
                ctx.beginPath();
                ctx.arc(star.x, star.y, currentRadius, 0, Math.PI * 2);
                ctx.fillStyle = `rgba(255, 255, 255, ${currentOpacity})`;
                ctx.fill();

                // Glow for larger or close stars
                if (currentRadius > 1.2 || interactionScale > 1.2) {
                    ctx.beginPath();
                    ctx.arc(star.x, star.y, currentRadius * 2.5, 0, Math.PI * 2);
                    const grad = ctx.createRadialGradient(
                        star.x, star.y, 0,
                        star.x, star.y, currentRadius * 2.5
                    );
                    grad.addColorStop(0, `rgba(160, 160, 255, ${currentOpacity * 0.3})`);
                    grad.addColorStop(1, 'rgba(160, 160, 255, 0)');
                    ctx.fillStyle = grad;
                    ctx.fill();
                }
            }

            rafRef.current = requestAnimationFrame(draw);
        };

        rafRef.current = requestAnimationFrame(draw);

        return () => {
            cancelAnimationFrame(rafRef.current);
            window.removeEventListener('resize', handleResize);
            window.removeEventListener('mousemove', handleMouseMove);
            window.removeEventListener('mouseleave', handleMouseLeave);
        };
    }, [initStars]);

    return (
        <canvas
            ref={canvasRef}
            style={{
                position: 'absolute',
                top: 0,
                left: 0,
                width: '100%',
                height: '100%',
                pointerEvents: 'none',
            }}
        />
    );
}

export default StarField;

