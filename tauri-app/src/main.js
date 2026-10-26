/**
 * CPDR 抓娃娃机 — 前端主逻辑
 *
 * 结构:
 *  - Joystick class: Canvas虚拟摇杆 (触摸+鼠标)
 *  - TauriAPI class: 封装Tauri invoke/event (离线模式降级)
 *  - UI update: 定时拉取状态并更新界面
 */

// ================================================================
// Tauri API 封装
// ================================================================

const isTauri = () => typeof window.__TAURI_INTERNALS__ !== 'undefined';

class TauriAPI {
  constructor() {
    this.ready = false;
    this.listeners = [];
  }

  async init() {
    if (!isTauri()) {
      console.log('[APP] 浏览器模式 (离线演示)');
      return;
    }
    try {
      // 动态导入 Tauri API
      const core = await import('@tauri-apps/api/core');
      const event = await import('@tauri-apps/api/event');
      this.invoke = core.invoke;
      this.listen = event.listen;
      this.ready = true;
      console.log('[APP] Tauri API 就绪');
    } catch (e) {
      console.warn('[APP] Tauri API 加载失败:', e.message);
    }
  }

  async call(cmd, args = {}) {
    if (this.ready) {
      try {
        return await this.invoke(cmd, args);
      } catch (e) {
        console.error(`[APP] invoke ${cmd} 失败:`, e);
        throw e;
      }
    }
    // 离线模式模拟
    console.log(`[APP] 离线模式 invoke: ${cmd}`, args);
    if (cmd === 'list_ports') return ['COM1', 'COM2', 'COM3'];
    if (cmd === 'get_status') return this.mockStatus();
    return null;
  }

  async onStatusUpdate(callback) {
    if (this.ready) {
      await this.listen('status-update', (event) => {
        callback(event.payload);
      });
    } else {
      // 浏览器模式: 用setInterval模拟
      setInterval(() => {
        callback(this.mockStatus());
      }, 200);
    }
  }

  mockStatus() {
    return {
      current_x: 0, current_y: 0,
      target_x: 0, target_y: 0,
      joystick_x: 0, joystick_y: 0,
      joystick_active: false,
      motors_enabled: false,
      motor_online: [false, false, false, false],
      motor_positions: [0, 0, 0, 0],
      motor_currents: [0, 0, 0, 0],
      estop: false,
      comm_errors: 0,
      motion_state: '离线演示',
      is_zeroed: false,
    };
  }
}

const api = new TauriAPI();

// ================================================================
// Canvas 虚拟摇杆
// ================================================================

class Joystick {
  constructor(canvasId) {
    this.canvas = document.getElementById(canvasId);
    this.ctx = this.canvas.getContext('2d');

    const w = this.canvas.width;
    const h = this.canvas.height;
    this.cx = w / 2;        // 底座中心 X
    this.cy = h / 2;        // 底座中心 Y
    this.baseR = 100;       // 底座半径
    this.knobR = 32;        // 旋钮半径
    this.maxOffset = this.baseR - this.knobR;

    this.knobX = 0;         // 旋钮相对中心偏移
    this.knobY = 0;
    this.active = false;
    this.range = 200.0;     // 映射到 ±200mm

    this.bindEvents();
    this.draw();
  }

  bindEvents() {
    const c = this.canvas;

    const getPos = (e) => {
      const r = c.getBoundingClientRect();
      const clientX = e.touches ? e.touches[0].clientX : e.clientX;
      const clientY = e.touches ? e.touches[0].clientY : e.clientY;
      return {
        x: clientX - r.left - this.cx,
        y: clientY - r.top - this.cy,
      };
    };

    const onStart = (e) => {
      e.preventDefault();
      this.active = true;
      this.moveKnob(getPos(e));
    };

    const onMove = (e) => {
      e.preventDefault();
      if (!this.active) return;
      this.moveKnob(getPos(e));
    };

    const onEnd = (e) => {
      e.preventDefault();
      this.active = false;
      this.knobX = 0;
      this.knobY = 0;
      this.draw();
      // 通知后端松手
      api.call('release_joystick').catch(() => {});
      uiUpdateJoystick(0, 0, false);
    };

    // 鼠标事件
    c.addEventListener('mousedown', onStart);
    window.addEventListener('mousemove', (e) => {
      if (this.active) onMove(e);
    });
    window.addEventListener('mouseup', onEnd);

    // 触摸事件
    c.addEventListener('touchstart', onStart, { passive: false });
    window.addEventListener('touchmove', (e) => {
      if (this.active) onMove(e);
    }, { passive: false });
    window.addEventListener('touchend', onEnd);
  }

  moveKnob(pos) {
    let dx = pos.x;
    let dy = pos.y;
    const dist = Math.sqrt(dx * dx + dy * dy);

    if (dist > this.maxOffset) {
      dx = (dx / dist) * this.maxOffset;
      dy = (dy / dist) * this.maxOffset;
    }

    this.knobX = dx;
    this.knobY = dy;
    this.draw();

    // 映射到控制范围 (±200mm)
    const tx = (dx / this.maxOffset) * this.range;
    const ty = -(dy / this.maxOffset) * this.range;
    uiUpdateJoystick(tx, ty, true);

    // 发送到后端
    api.call('set_joystick_target', { x: tx, y: ty }).catch(() => {});
  }

  draw() {
    const ctx = this.ctx;
    const w = this.canvas.width;
    const h = this.canvas.height;
    ctx.clearRect(0, 0, w, h);

    // 底色
    ctx.fillStyle = '#16213E';
    ctx.beginPath();
    ctx.arc(this.cx, this.cy, this.baseR, 0, Math.PI * 2);
    ctx.fill();

    // 边框
    ctx.strokeStyle = '#0F3460';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(this.cx, this.cy, this.baseR, 0, Math.PI * 2);
    ctx.stroke();

    // 十字参考线
    ctx.strokeStyle = '#333';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(this.cx - this.baseR + 10, this.cy);
    ctx.lineTo(this.cx + this.baseR - 10, this.cy);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(this.cx, this.cy - this.baseR + 10);
    ctx.lineTo(this.cx, this.cy + this.baseR - 10);
    ctx.stroke();

    // 旋钮
    const kx = this.cx + this.knobX;
    const ky = this.cy + this.knobY;
    ctx.fillStyle = '#E94560';
    ctx.beginPath();
    ctx.arc(kx, ky, this.knobR, 0, Math.PI * 2);
    ctx.fill();
  }
}

// ================================================================
// UI 更新函数
// ================================================================

function uiUpdateJoystick(x, y, active) {
  document.getElementById('joystick-value').textContent =
    `X: ${x.toFixed(1)} mm   Y: ${y.toFixed(1)} mm${active ? '' : ' (松手)'}`;
}

function updateStatus(s) {
  // 当前位置
  document.getElementById('val-current').textContent =
    `X: ${s.current_x.toFixed(1)} mm  Y: ${s.current_y.toFixed(1)} mm`;

  // 目标位置
  document.getElementById('val-target').textContent =
    `X: ${s.target_x.toFixed(1)} mm  Y: ${s.target_y.toFixed(1)} mm`;

  // 电机在线状态
  const motorLabels = ['m1', 'm2', 'm3', 'm4'];
  for (let i = 0; i < 4; i++) {
    const dot = document.getElementById(`${motorLabels[i]}-dot`);
    if (dot) {
      dot.className = 'motor-dot ' + (s.motor_online[i] ? 'online' : 'offline');
    }
  }

  // 通信错误
  document.getElementById('val-comm').textContent = `错误: ${s.comm_errors}`;

  // 归零状态
  document.getElementById('val-zero').textContent = s.is_zeroed ? '已归零' : '未归零';

  // 状态
  document.getElementById('status-text').textContent = s.motion_state;

  // 连接按钮
  const btnConn = document.getElementById('btn-connect');
  if (btnConn) {
    btnConn.className = s.motor_online.some(o => o) ? 'connected' : '';
    btnConn.textContent = s.motor_online.some(o => o) ? '已连接' : '连接';
  }
}

// ================================================================
// 按钮事件
// ================================================================

function bindButtons() {
  document.getElementById('btn-enable').addEventListener('click', async () => {
    try { await api.call('enable_motors'); } catch (e) { alert('使能失败: ' + e); }
  });

  document.getElementById('btn-disable').addEventListener('click', async () => {
    try { await api.call('disable_motors'); } catch (e) { alert('失能失败: ' + e); }
  });

  document.getElementById('btn-zero').addEventListener('click', async () => {
    try { await api.call('zero_position'); } catch (e) { alert('归零失败: ' + e); }
  });

  document.getElementById('btn-grab').addEventListener('click', async () => {
    try { await api.call('grab'); } catch (e) { alert('抓取失败: ' + e); }
  });

  document.getElementById('btn-estop').addEventListener('click', async () => {
    try { await api.call('emergency_stop'); } catch (e) { alert('急停失败: ' + e); }
  });

  // 串口连接
  document.getElementById('btn-connect').addEventListener('click', async () => {
    const sel = document.getElementById('port-list');
    const port = sel.value;
    if (!port) return alert('请选择串口');
    try {
      await api.call('connect_port', { portName: port });
      document.getElementById('btn-connect').className = 'connected';
      document.getElementById('btn-connect').textContent = '已连接';
    } catch (e) {
      alert('连接失败: ' + e);
    }
  });

  // 串口列表刷新
  document.getElementById('port-list').addEventListener('focus', async () => {
    try {
      const ports = await api.call('list_ports');
      const sel = document.getElementById('port-list');
      sel.innerHTML = ports.map(p => `<option value="${p}">${p}</option>`).join('');
      if (ports.length === 0) {
        sel.innerHTML = '<option value="">无可用串口</option>';
      }
    } catch (e) { /* 忽略 */ }
  });
}

// ================================================================
// 初始化
// ================================================================

(async function main() {
  await api.init();
  bindButtons();
  const joystick = new Joystick('joystick-canvas');

  // 首次加载串口列表
  try {
    const ports = await api.call('list_ports');
    const sel = document.getElementById('port-list');
    if (ports && ports.length > 0) {
      sel.innerHTML = ports.map(p => `<option value="${p}">${p}</option>`).join('');
    }
  } catch (e) { /* 忽略 */ }

  // 持续接收状态更新
  api.onStatusUpdate((status) => {
    updateStatus(status);
  });

  console.log('[APP] 前端初始化完成');
})();
