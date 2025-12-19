Page({
  data: {
    connected: false,
    stickStyle: '',
    speedStyle: 'width: 70%;',
    gear: 180,
    driftMode: false,
    lastCmdName: '停止',
    lastDir: 'stop'
  },
  socket: null,
  rect: null,

  onReady() { this.initCoordinates(); this.initSocket(); },

  initSocket() {
    this.socket = wx.connectSocket({ url: 'ws://10.46.83.27:81' });
    this.socket.onOpen(() => this.setData({ connected: true }));
    this.socket.onClose(() => {
      this.setData({ connected: false });
      setTimeout(() => this.initSocket(), 3000);
    });
  },

  toggleDrift() {
    this.setData({ driftMode: !this.data.driftMode });
    wx.vibrateLong(); // 震动反馈
  },

  setGear(e) {
    const val = parseInt(e.currentTarget.dataset.val);
    this.setData({ 
      gear: val,
      speedStyle: `width: ${Math.floor(val/2.55)}%;`
    });
    wx.vibrateShort({ type: 'medium' });
  },

  initCoordinates() {
    wx.createSelectorQuery().select('#base').boundingClientRect(rect => {
      if (rect) {
        this.rect = rect;
        const centerX = rect.width / 2;
        const stickS = rect.width * 120 / 320;
        const p = centerX - (stickS / 2);
        this.setData({ stickStyle: `left:${p}px; top:${p}px;` });
      }
    }).exec();
  },

  touchMove(e) {
    if (!this.rect) return;
    const touch = e.touches[0];
    const centerX = this.rect.width / 2;
    const centerY = this.rect.height / 2;
    const stickS = this.rect.width * 120 / 320;
    let dx = touch.clientX - this.rect.left - centerX;
    let dy = touch.clientY - this.rect.top - centerY;
    const dist = Math.sqrt(dx * dx + dy * dy);
    const maxR = centerX - 10;

    if (dist > maxR) { dx *= (maxR / dist); dy *= (maxR / dist); }

    this.setData({
      stickStyle: `left:${centerX + dx - (stickS/2)}px; top:${centerY + dy - (stickS/2)}px;`
    });

    if (dist < 30) {
      this.updateMove('stop');
    } else {
      const angle = Math.atan2(dy, dx) * 180 / Math.PI;
      let dir = "";
      if (angle > -45 && angle <= 45) dir = "right";
      else if (angle > 45 && angle <= 135) dir = "backward";
      else if (angle > 135 || angle <= -135) dir = "left";
      else dir = "forward";
      this.updateMove(dir);
    }
  },

  updateMove(dir) {
    if (this.data.lastDir !== dir) {
      let finalDir = dir;
      let speed = (dir === 'stop') ? 0 : this.data.gear;

      // 漂移逻辑：左右转向时如果开启漂移模式，改用满额功率并改变指令名
      if (this.data.driftMode && (dir === 'left' || dir === 'right')) {
        speed = 255; 
        finalDir = (dir === 'left') ? 'drift_l' : 'drift_r';
      }

      this.sendMsg(finalDir, speed);
      this.setData({ lastDir: dir, lastCmdName: finalDir });
      wx.vibrateShort({ type: 'light' });
    }
  },

  sendMsg(dir, speed) {
    if (this.data.connected) this.socket.send({ data: `${dir}:${speed}` });
  },

  touchEnd() { this.initCoordinates(); this.updateMove('stop'); }
});