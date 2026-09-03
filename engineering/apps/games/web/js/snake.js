/**
 * 贪吃蛇游戏控制器
 * Canvas 渲染 + 键盘输入 + requestAnimationFrame 帧循环
 */
class SnakeGame {
    constructor(canvasId, engine) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.engine = engine;
        this.gridSize = 20;   // 每格 20px，20×20 = 400px
        this.lastTick = 0;
        this.interval = 180;  // 帧间隔 ms
        this.rafId = null;
    }

    start(seed = Date.now(), difficulty = 0) {
        this.engine.snake_create(seed, difficulty);
        this.interval = difficulty === 0 ? 180 : difficulty === 1 ? 120 : 80;
        this.render();
        this.bindInput();
        this.loop();
    }

    loop(timestamp = 0) {
        const elapsed = timestamp - this.lastTick;
        if (elapsed >= this.interval) {
            this.engine.snake_tick();
            this.render();
            this.lastTick = timestamp;

            if (this.engine.snake_over()) {
                this.showGameOver();
                return;
            }
        }
        this.rafId = requestAnimationFrame((t) => this.loop(t));
    }

    render() {
        const ctx = this.ctx;
        const gs = this.gridSize;

        // 清空
        ctx.fillStyle = '#fff';
        ctx.fillRect(0, 0, 20 * gs, 20 * gs);

        // 网格（可选）
        ctx.strokeStyle = '#eee';
        for (let i = 0; i <= 20; i++) {
            ctx.beginPath();
            ctx.moveTo(i * gs, 0);
            ctx.lineTo(i * gs, 20 * gs);
            ctx.moveTo(0, i * gs);
            ctx.lineTo(20 * gs, i * gs);
            ctx.stroke();
        }

        // 食物
        ctx.fillStyle = '#e74c3c';
        const fx = this.engine.snake_food_x();
        const fy = this.engine.snake_food_y();
        ctx.fillRect(fx * gs + 1, fy * gs + 1, gs - 2, gs - 2);

        // 蛇身
        ctx.fillStyle = '#2ecc71';
        const count = this.engine.snake_body_count();
        for (let i = 0; i < count; i++) {
            const x = this.engine.snake_body_x(i);
            const y = this.engine.snake_body_y(i);
            ctx.fillRect(x * gs + 1, y * gs + 1, gs - 2, gs - 2);
        }

        // 分数
        document.getElementById('score-val').textContent = this.engine.snake_score_val();
        document.getElementById('speed-val').textContent = this.interval;
    }

    bindInput() {
        document.addEventListener('keydown', (e) => {
            const dirMap = {
                'ArrowUp': 0, 'w': 0, 'W': 0,
                'ArrowDown': 1, 's': 1, 'S': 1,
                'ArrowLeft': 2, 'a': 2, 'A': 2,
                'ArrowRight': 3, 'd': 3, 'D': 3,
            };
            const dir = dirMap[e.key];
            if (dir !== undefined) {
                e.preventDefault();
                this.engine.snake_input_dir(dir);
            }
        });
    }

    showGameOver() {
        cancelAnimationFrame(this.rafId);
        document.getElementById('final-score').textContent = this.engine.snake_score_val();
        document.getElementById('overlay').classList.remove('hidden');
    }

    restart() {
        document.getElementById('overlay').classList.add('hidden');
        this.start(Date.now(), 0);
    }
}