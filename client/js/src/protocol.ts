export const ICM_IPC_VERSION = 2;
export const ICM_MAX_FDS_PER_MSG = 4;

export enum IcmIpcMsgType {
  CREATE_WINDOW = 1,
  DESTROY_WINDOW = 2,
  SET_WINDOW = 3,
  SET_LAYER = 4,
  SET_ATTACHMENTS = 5,
  DRAW_RECT = 6,
  CLEAR_RECTS = 7,
  IMPORT_DMABUF = 8,
  EXPORT_DMABUF = 9,
  DRAW_LINE = 10,
  DRAW_CIRCLE = 11,
  DRAW_POLYGON = 12,
  DRAW_IMAGE = 13,
  BLIT_BUFFER = 14,
  BATCH_BEGIN = 15,
  BATCH_END = 16,
  EXPORT_SURFACE = 17,
  IMPORT_SURFACE = 18,
  CREATE_BUFFER = 19,
  DESTROY_BUFFER = 20,
  QUERY_BUFFER_INFO = 21,
  REGISTER_POINTER_EVENT = 22,
  REGISTER_KEYBOARD_EVENT = 23,
  QUERY_CAPTURE_MOUSE = 24,
  QUERY_CAPTURE_KEYBOARD = 25,
  POINTER_EVENT = 26,
  KEYBOARD_EVENT = 27,
  UPLOAD_IMAGE = 28,
  DESTROY_IMAGE = 29,
  DRAW_UPLOADED_IMAGE = 30,
  DRAW_TEXT = 31,
  SET_WINDOW_VISIBLE = 32,
  REGISTER_KEYBIND = 33,
  UNREGISTER_KEYBIND = 34,
  KEYBIND_EVENT = 35,
  WINDOW_CREATED = 36,
  WINDOW_DESTROYED = 37,
  REGISTER_CLICK_REGION = 38,
  UNREGISTER_CLICK_REGION = 39,
  CLICK_REGION_EVENT = 40,
  REQUEST_SCREEN_COPY = 41,
  SCREEN_COPY_DATA = 42,
  REGISTER_GLOBAL_POINTER_EVENT = 43,
  REGISTER_GLOBAL_KEYBOARD_EVENT = 44,
  REGISTER_GLOBAL_CAPTURE_MOUSE = 45,
  REGISTER_GLOBAL_CAPTURE_KEYBOARD = 46,
  UNREGISTER_GLOBAL_CAPTURE_KEYBOARD = 58,
  UNREGISTER_GLOBAL_CAPTURE_MOUSE = 59,
  SET_WINDOW_POSITION = 47,
  SET_WINDOW_SIZE = 48,
  SET_WINDOW_OPACITY = 49,
  SET_WINDOW_TRANSFORM = 50,
  SET_WINDOW_BLUR = 78,
  SET_SCREEN_EFFECT = 79,
  SET_WINDOW_EFFECT = 80,
  SET_WINDOW_LAYER = 60,
  RAISE_WINDOW = 61,
  LOWER_WINDOW = 62,
  SET_WINDOW_PARENT = 63,
  SET_WINDOW_TRANSFORM_3D = 64,
  SET_WINDOW_MATRIX = 65,
  SET_WINDOW_STATE = 66,
  FOCUS_WINDOW = 67,
  BLUR_WINDOW = 83,
  QUERY_WINDOW_POSITION = 52,
  QUERY_WINDOW_SIZE = 53,
  QUERY_WINDOW_ATTRIBUTES = 54,
  QUERY_WINDOW_LAYER = 68,
  QUERY_WINDOW_STATE = 69,
  WINDOW_POSITION_DATA = 55,
  WINDOW_SIZE_DATA = 56,
  WINDOW_ATTRIBUTES_DATA = 57,
  WINDOW_LAYER_DATA = 70,
  WINDOW_STATE_DATA = 71,
  QUERY_SCREEN_DIMENSIONS = 72,
  SCREEN_DIMENSIONS_DATA = 73,
  QUERY_MONITORS = 74,
  MONITORS_DATA = 75,
  COMPOSITOR_SHUTDOWN = 51,
  QUERY_WINDOW_INFO = 76,
  WINDOW_INFO_DATA = 77,
  ANIMATE_WINDOW = 81,
  STOP_ANIMATION = 82
}

export interface IcmIpcHeader {
  length: number; // Total message length including header
  type: number;
  flags: number;
  sequence: number; // For matching replies
  numFds: number; // Number of file descriptors following ts
}

export interface IcmMsgCreateBuffer {
  bufferId: number;
  width: number;
  height: number;
  format: number; // DRM format code
  usageFlags: number; // GPU, CPU memory, whatever
}

export interface IcmMsgDestroyBuffer {
  bufferId: number;
}

export interface IcmMsgDrawRect {
  windowId: number;
  x: number;
  y: number;
  width: number;
  height: number;
  colorRgba: number;
}

export interface IcmMsgRegisterPointerEvent {
  windowId: number;
}

export interface IcmMsgRegisterKeyboardEvent {
  windowId: number;
}

export interface IcmMsgQueryCaptureMouse {
  windowId: number;
}

export interface IcmMsgQueryCaptureKeyboard {
  windowId: number;
}

export interface IcmMsgPointerEvent {
  windowId: number;
  time: number;
  button: number;
  state: number;
  x: number;
  y: number;
}

export interface IcmMsgKeyboardEvent {
  windowId: number;
  time: number;
  keycode: number;
  state: number;
  modifiers: number;  /* Modifier keys (e.g., Shift, Ctrl, Alt) */
}

export interface IcmMsgUploadImage {
  imageId: number;
  width: number;
  height: number;
  format: number;
  data: Uint8Array;
}

export interface IcmMsgDestroyImage {
  imageId: number;
}

export interface IcmMsgDrawUploadedImage {
  windowId: number;
  imageId: number;
  x: number;
  y: number;
  width: number;
  height: number;
  srcX: number;
  srcY: number;
  srcWidth: number;
  srcHeight: number;
  alpha: number;
}

export interface IcmMsgDrawText {
  windowId: number;
  x: number;
  y: number;
  colorRgba: number;
  fontSize: number;
  text: string;
}

export interface IcmMsgSetWindowVisible {
  windowId: number;
  visible: boolean;
}

export interface IcmMsgRegisterKeybind {
  keybindId: number;
  modifiers: number;
  keycode: number;
}

export interface IcmMsgUnregisterKeybind {
  keybindId: number;
}

export interface IcmMsgKeybindEvent {
  keybindId: number;
}

export interface IcmMsgWindowCreated {
  windowId: number;
  width: number;
  height: number;
  decorated: boolean;
  focused: boolean;
}

export interface IcmMsgWindowDestroyed {
  windowId: number;
}

export interface IcmMsgRegisterClickRegion {
  windowId: number;
  regionId: number;
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface IcmMsgUnregisterClickRegion {
  regionId: number;
}

export interface IcmMsgClickRegionEvent {
  regionId: number;
  button: number;
  state: number;
}

export interface IcmMsgRequestScreenCopy {
  requestId: number;
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface IcmMsgScreenCopyData {
  requestId: number;
  width: number;
  height: number;
  format: number;
  data: Uint8Array;
}

export interface IcmMsgRegisterGlobalPointerEvent {
  // No fields
}

export interface IcmMsgRegisterGlobalKeyboardEvent {
  // No fields
}

export interface IcmMsgRegisterGlobalCaptureMouse {
  // No fields
}

export interface IcmMsgRegisterGlobalCaptureKeyboard {
  // No fields
}

export interface IcmMsgUnregisterGlobalCaptureKeyboard {
  // No fields
}

export interface IcmMsgUnregisterGlobalCaptureMouse {
  // No fields
}

export interface IcmMsgSetWindowPosition {
  windowId: number;
  x: number;
  y: number;
}

export interface IcmMsgSetWindowSize {
  windowId: number;
  width: number;
  height: number;
}

export interface IcmMsgSetWindowOpacity {
  windowId: number;
  opacity: number;
}

export interface IcmMsgSetWindowBlur {
  windowId: number;
  blurRadius: number;
  enabled: boolean;
}

export interface IcmMsgSetScreenEffect {
  equation: string;
  enabled: boolean;
}

export interface IcmMsgSetWindowEffect {
  windowId: number;
  equation: string;
  enabled: boolean;
}

export interface IcmMsgSetWindowTransform {
  windowId: number;
  scaleX: number;
  scaleY: number;
  rotation: number;
}

export interface IcmMsgSetWindowLayer {
  windowId: number;
  layer: number;
}

export interface IcmMsgRaiseWindow {
  windowId: number;
}

export interface IcmMsgLowerWindow {
  windowId: number;
}

export interface IcmMsgSetWindowParent {
  windowId: number;
  parentId: number;
}

export interface IcmMsgSetWindowTransform3D {
  windowId: number;
  translateX: number;
  translateY: number;
  translateZ: number;
  rotateX: number;
  rotateY: number;
  rotateZ: number;
  scaleX: number;
  scaleY: number;
  scaleZ: number;
}

export interface IcmMsgSetWindowMatrix {
  windowId: number;
  matrix: number[]; // 16-element array for 4x4 matrix
}

export interface IcmMsgAnimateWindow {
  windowId: number;
  durationMs: number;
  targetX: number;
  targetY: number;
  targetScaleX: number;
  targetScaleY: number;
  targetOpacity: number;
  targetTranslateX: number;
  targetTranslateY: number;
  targetTranslateZ: number;
  targetRotateX: number;
  targetRotateY: number;
  targetRotateZ: number;
  targetScaleZ: number;
  flags: number; // bitfield: 1=animate position, 2=animate scale, 4=animate opacity, 8=animate 3d translate, 16=animate 3d rotate, 32=animate 3d scale
}

export interface IcmMsgStopAnimation {
  windowId: number;
}

export interface IcmMsgSetWindowState {
  windowId: number;
  state: number; // bitfield
}

export interface IcmMsgFocusWindow {
  windowId: number;
}

export interface IcmMsgBlurWindow {
  windowId: number;
}

export interface IcmMsgQueryWindowPosition {
  windowId: number;
}

export interface IcmMsgQueryWindowSize {
  windowId: number;
}

export interface IcmMsgQueryWindowAttributes {
  windowId: number;
}

export interface IcmMsgQueryWindowLayer {
  windowId: number;
}

export interface IcmMsgQueryWindowState {
  windowId: number;
}

export interface IcmMsgWindowPositionData {
  windowId: number;
  x: number;
  y: number;
}

export interface IcmMsgWindowSizeData {
  windowId: number;
  width: number;
  height: number;
}

export interface IcmMsgWindowAttributesData {
  windowId: number;
  visible: boolean;
  opacity: number;
  scaleX: number;
  scaleY: number;
  rotation: number;
}

export interface IcmMsgWindowLayerData {
  windowId: number;
  layer: number;
  parentId: number;
}

export interface IcmMsgWindowStateData {
  windowId: number;
  state: number;
  focused: boolean;
}

export interface IcmMsgQueryScreenDimensions {
  // No payload
}

export interface IcmMsgScreenDimensionsData {
  totalWidth: number;
  totalHeight: number;
  scale: number;
}

export interface IcmMsgQueryWindowInfo {
  windowId: number;
}

export interface IcmMsgWindowInfoData {
  windowId: number;
  x: number;
  y: number;
  width: number;
  height: number;
  visible: boolean;
  opacity: number;
  scaleX: number;
  scaleY: number;
  rotation: number;
  layer: number;
  parentId: number;
  state: number;
  focused: boolean;
  pid: number;
  processName: string;
  title: string; // Alias for processName for convenience
}

export interface IcmMonitorInfo {
  x: number;
  y: number;
  width: number;
  height: number;
  physicalWidth: number;
  physicalHeight: number;
  refreshRate: number;
  scale: number;
  enabled: boolean;
  primary: boolean;
  name: string;
}

export interface IcmMsgQueryMonitors {
  // No payload
}

export interface IcmMsgMonitorsData {
  monitors: IcmMonitorInfo[];
}

export interface IcmMsgCompositorShutdown {
}

export function createHeader(type: number, payloadSize: number): IcmIpcHeader {
  return {
    length: 16 + payloadSize, // Header is 16 bytes
    type,
    flags: 0,
    sequence: 0,
    numFds: 0
  };
}

export function serializeMessage(header: IcmIpcHeader, payload: Buffer | null): Buffer {
  const headerBuf = Buffer.alloc(16); // Header is 16 bytes
  headerBuf.writeUInt32LE(header.length, 0);
  headerBuf.writeUInt16LE(header.type, 4);
  headerBuf.writeUInt16LE(header.flags, 6);
  headerBuf.writeUInt32LE(header.sequence, 8);
  headerBuf.writeInt32LE(header.numFds, 12);

  if (payload) {
    return Buffer.concat([headerBuf, payload]);
  }
  return headerBuf;
}

export function serializeCreateBuffer(msg: IcmMsgCreateBuffer): Buffer {
  const buf = Buffer.alloc(20); // 5 uint32
  buf.writeUInt32LE(msg.bufferId, 0);
  buf.writeUInt32LE(msg.width, 4);
  buf.writeUInt32LE(msg.height, 8);
  buf.writeUInt32LE(msg.format, 12);
  buf.writeUInt32LE(msg.usageFlags, 16);
  return buf;
}

export function serializeDestroyBuffer(msg: IcmMsgDestroyBuffer): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.bufferId, 0);
  return buf;
}

export function serializeDrawRect(msg: IcmMsgDrawRect): Buffer {
  const buf = Buffer.alloc(24); // 6 uint32
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeUInt32LE(msg.x, 4);
  buf.writeUInt32LE(msg.y, 8);
  buf.writeUInt32LE(msg.width, 12);
  buf.writeUInt32LE(msg.height, 16);
  buf.writeUInt32LE(msg.colorRgba, 20);
  return buf;
}

export function serializeRegisterPointerEvent(msg: IcmMsgRegisterPointerEvent): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeRegisterKeyboardEvent(msg: IcmMsgRegisterKeyboardEvent): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function deserializePointerEvent(buf: Buffer): IcmMsgPointerEvent {
  return {
    windowId: buf.readUInt32LE(0),
    time: buf.readUInt32LE(4),
    button: buf.readUInt32LE(8),
    state: buf.readUInt32LE(12),
    x: buf.readInt32LE(16),
    y: buf.readInt32LE(20)
  };
}

export function deserializeKeyboardEvent(buf: Buffer): IcmMsgKeyboardEvent {
  return {
    windowId: buf.readUInt32LE(0),
    time: buf.readUInt32LE(4),
    keycode: buf.readUInt32LE(8),
    state: buf.readUInt32LE(12),
    modifiers: buf.readUInt32LE(16)
  };
}

export function serializeUploadImage(msg: IcmMsgUploadImage): Buffer {
  const buf = Buffer.alloc(20 + msg.data.length);
  buf.writeUInt32LE(msg.imageId, 0);
  buf.writeUInt32LE(msg.width, 4);
  buf.writeUInt32LE(msg.height, 8);
  buf.writeUInt32LE(msg.format, 12);
  buf.writeUInt32LE(msg.data.length, 16);
  buf.set(msg.data, 20);
  return buf;
}

export function serializeDestroyImage(msg: IcmMsgDestroyImage): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.imageId, 0);
  return buf;
}

export function serializeDrawUploadedImage(msg: IcmMsgDrawUploadedImage): Buffer {
  const buf = Buffer.alloc(32);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeUInt32LE(msg.imageId, 4);
  buf.writeInt32LE(msg.x, 8);
  buf.writeInt32LE(msg.y, 12);
  buf.writeUInt32LE(msg.width, 16);
  buf.writeUInt32LE(msg.height, 20);
  buf.writeUInt32LE(msg.srcX, 24);
  buf.writeUInt32LE(msg.srcY, 28);
  buf.writeUInt32LE(msg.srcWidth, 32);
  buf.writeUInt32LE(msg.srcHeight, 36);
  buf.writeUInt8(msg.alpha, 40);
  return buf;
}

export function serializeDrawText(msg: IcmMsgDrawText): Buffer {
  const textBuf = Buffer.from(msg.text, 'utf8');
  const buf = Buffer.alloc(20 + textBuf.length);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeInt32LE(msg.x, 4);
  buf.writeInt32LE(msg.y, 8);
  buf.writeUInt32LE(msg.colorRgba, 12);
  buf.writeUInt32LE(msg.fontSize, 16);
  textBuf.copy(buf, 20);
  return buf;
}
export function serializeSetWindowVisible(msg: IcmMsgSetWindowVisible): Buffer {
  const buf = Buffer.alloc(5);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeUInt8(msg.visible ? 1 : 0, 4);
  return buf;
}

export function serializeRegisterKeybind(msg: IcmMsgRegisterKeybind): Buffer {
  const buf = Buffer.alloc(12);
  buf.writeUInt32LE(msg.keybindId, 0);
  buf.writeUInt32LE(msg.modifiers, 4);
  buf.writeUInt32LE(msg.keycode, 8);
  return buf;
}

export function serializeUnregisterKeybind(msg: IcmMsgUnregisterKeybind): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.keybindId, 0);
  return buf;
}

export function deserializeKeybindEvent(buf: Buffer): IcmMsgKeybindEvent {
  return {
    keybindId: buf.readUInt32LE(0)
  };
}

export function deserializeWindowCreated(buf: Buffer): IcmMsgWindowCreated {
  if (buf.length < 14) {
    console.error(`WindowCreated: buffer too small (${buf.length} bytes, expected 14)`);
    throw new Error(`Buffer too small for WindowCreated: ${buf.length}`);
  }
  return {
    windowId: buf.readUInt32LE(0),
    width: buf.readUInt32LE(4),
    height: buf.readUInt32LE(8),
    decorated: buf.readUInt8(12) !== 0,
    focused: buf.readUInt8(13) !== 0
  };
}

export function deserializeWindowDestroyed(buf: Buffer): IcmMsgWindowDestroyed {
  return {
    windowId: buf.readUInt32LE(0)
  };
}

export function serializeRegisterClickRegion(msg: IcmMsgRegisterClickRegion): Buffer {
  const buf = Buffer.alloc(24);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeUInt32LE(msg.regionId, 4);
  buf.writeInt32LE(msg.x, 8);
  buf.writeInt32LE(msg.y, 12);
  buf.writeUInt32LE(msg.width, 16);
  buf.writeUInt32LE(msg.height, 20);
  return buf;
}

export function serializeUnregisterClickRegion(msg: IcmMsgUnregisterClickRegion): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.regionId, 0);
  return buf;
}

export function deserializeClickRegionEvent(buf: Buffer): IcmMsgClickRegionEvent {
  return {
    regionId: buf.readUInt32LE(0),
    button: buf.readUInt32LE(4),
    state: buf.readUInt32LE(8)
  };
}

export function serializeRequestScreenCopy(msg: IcmMsgRequestScreenCopy): Buffer {
  const buf = Buffer.alloc(20);
  buf.writeUInt32LE(msg.requestId, 0);
  buf.writeUInt32LE(msg.x, 4);
  buf.writeUInt32LE(msg.y, 8);
  buf.writeUInt32LE(msg.width, 12);
  buf.writeUInt32LE(msg.height, 16);
  return buf;
}

export function deserializeScreenCopyData(buf: Buffer): IcmMsgScreenCopyData {
  const dataSize = buf.readUInt32LE(12);
  return {
    requestId: buf.readUInt32LE(0),
    width: buf.readUInt32LE(4),
    height: buf.readUInt32LE(8),
    format: buf.readUInt32LE(12),
    data: buf.slice(16, 16 + dataSize)
  };
}

export function serializeRegisterGlobalPointerEvent(msg: IcmMsgRegisterGlobalPointerEvent): Buffer {
  return Buffer.alloc(0);
}

export function serializeRegisterGlobalKeyboardEvent(msg: IcmMsgRegisterGlobalKeyboardEvent): Buffer {
  return Buffer.alloc(0);
}

export function serializeRegisterGlobalCaptureMouse(msg: IcmMsgRegisterGlobalCaptureMouse): Buffer {
  return Buffer.alloc(0);
}

export function serializeRegisterGlobalCaptureKeyboard(msg: IcmMsgRegisterGlobalCaptureKeyboard): Buffer {
  return Buffer.alloc(0);
}

export function serializeUnregisterGlobalCaptureKeyboard(msg: IcmMsgUnregisterGlobalCaptureKeyboard): Buffer {
  return Buffer.alloc(0);
}

export function serializeUnregisterGlobalCaptureMouse(msg: IcmMsgUnregisterGlobalCaptureMouse): Buffer {
  return Buffer.alloc(0);
}

export function serializeSetWindowPosition(msg: IcmMsgSetWindowPosition): Buffer {
  const buf = Buffer.alloc(12);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeInt32LE(msg.x, 4);
  buf.writeInt32LE(msg.y, 8);
  return buf;
}

export function serializeSetWindowSize(msg: IcmMsgSetWindowSize): Buffer {
  const buf = Buffer.alloc(12);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeUInt32LE(msg.width, 4);
  buf.writeUInt32LE(msg.height, 8);
  return buf;
}

export function serializeSetWindowOpacity(msg: IcmMsgSetWindowOpacity): Buffer {
  const buf = Buffer.alloc(8);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeFloatLE(msg.opacity, 4);
  return buf;
}

export function serializeSetWindowBlur(msg: IcmMsgSetWindowBlur): Buffer {
  const buf = Buffer.alloc(9);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeFloatLE(msg.blurRadius, 4);
  buf.writeUInt8(msg.enabled ? 1 : 0, 8);
  return buf;
}

export function serializeSetScreenEffect(msg: IcmMsgSetScreenEffect): Buffer {
  const buf = Buffer.alloc(257);
  buf.write(msg.equation, 0, 256, 'utf8');
  buf.writeUInt8(msg.enabled ? 1 : 0, 256);
  return buf;
}

export function serializeSetWindowEffect(msg: IcmMsgSetWindowEffect): Buffer {
  const buf = Buffer.alloc(261);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.write(msg.equation, 4, 256, 'utf8');
  buf.writeUInt8(msg.enabled ? 1 : 0, 260);
  return buf;
}

export function serializeSetWindowTransform(msg: IcmMsgSetWindowTransform): Buffer {
  const buf = Buffer.alloc(20);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeFloatLE(msg.scaleX, 4);
  buf.writeFloatLE(msg.scaleY, 8);
  buf.writeFloatLE(msg.rotation, 12);
  return buf;
}

export function serializeSetWindowLayer(msg: IcmMsgSetWindowLayer): Buffer {
  const buf = Buffer.alloc(8);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeInt32LE(msg.layer, 4);
  return buf;
}

export function serializeRaiseWindow(msg: IcmMsgRaiseWindow): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeLowerWindow(msg: IcmMsgLowerWindow): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeSetWindowParent(msg: IcmMsgSetWindowParent): Buffer {
  const buf = Buffer.alloc(8);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeUInt32LE(msg.parentId, 4);
  return buf;
}

export function serializeSetWindowTransform3D(msg: IcmMsgSetWindowTransform3D): Buffer {
  const buf = Buffer.alloc(40);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeFloatLE(msg.translateX, 4);
  buf.writeFloatLE(msg.translateY, 8);
  buf.writeFloatLE(msg.translateZ, 12);
  buf.writeFloatLE(msg.rotateX, 16);
  buf.writeFloatLE(msg.rotateY, 20);
  buf.writeFloatLE(msg.rotateZ, 24);
  buf.writeFloatLE(msg.scaleX, 28);
  buf.writeFloatLE(msg.scaleY, 32);
  buf.writeFloatLE(msg.scaleZ, 36);
  return buf;
}

export function serializeSetWindowMatrix(msg: IcmMsgSetWindowMatrix): Buffer {
  const buf = Buffer.alloc(68); // 4 + 16*4
  buf.writeUInt32LE(msg.windowId, 0);
  for (let i = 0; i < 16; i++) {
    buf.writeFloatLE(msg.matrix[i], 4 + i * 4);
  }
  return buf;
}

export function serializeAnimateWindow(msg: IcmMsgAnimateWindow): Buffer {
  const buf = Buffer.alloc(60);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeUInt32LE(msg.durationMs, 4);
  buf.writeFloatLE(msg.targetX, 8);
  buf.writeFloatLE(msg.targetY, 12);
  buf.writeFloatLE(msg.targetScaleX, 16);
  buf.writeFloatLE(msg.targetScaleY, 20);
  buf.writeFloatLE(msg.targetOpacity, 24);
  buf.writeFloatLE(msg.targetTranslateX, 28);
  buf.writeFloatLE(msg.targetTranslateY, 32);
  buf.writeFloatLE(msg.targetTranslateZ, 36);
  buf.writeFloatLE(msg.targetRotateX, 40);
  buf.writeFloatLE(msg.targetRotateY, 44);
  buf.writeFloatLE(msg.targetRotateZ, 48);
  buf.writeFloatLE(msg.targetScaleZ, 52);
  buf.writeUInt32LE(msg.flags, 56);
  return buf;
}

export function serializeStopAnimation(msg: IcmMsgStopAnimation): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeSetWindowState(msg: IcmMsgSetWindowState): Buffer {
  const buf = Buffer.alloc(8);
  buf.writeUInt32LE(msg.windowId, 0);
  buf.writeUInt32LE(msg.state, 4);
  return buf;
}

export function serializeFocusWindow(msg: IcmMsgFocusWindow): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeBlurWindow(msg: IcmMsgBlurWindow): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeQueryWindowPosition(msg: IcmMsgQueryWindowPosition): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeQueryWindowSize(msg: IcmMsgQueryWindowSize): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeQueryWindowAttributes(msg: IcmMsgQueryWindowAttributes): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeQueryWindowLayer(msg: IcmMsgQueryWindowLayer): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeQueryWindowState(msg: IcmMsgQueryWindowState): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function serializeQueryWindowInfo(msg: IcmMsgQueryWindowInfo): Buffer {
  const buf = Buffer.alloc(4);
  buf.writeUInt32LE(msg.windowId, 0);
  return buf;
}

export function deserializeWindowPositionData(buf: Buffer): IcmMsgWindowPositionData {
  return {
    windowId: buf.readUInt32LE(0),
    x: buf.readInt32LE(4),
    y: buf.readInt32LE(8)
  };
}

export function deserializeWindowSizeData(buf: Buffer): IcmMsgWindowSizeData {
  return {
    windowId: buf.readUInt32LE(0),
    width: buf.readUInt32LE(4),
    height: buf.readUInt32LE(8)
  };
}

export function deserializeWindowInfoData(buf: Buffer): IcmMsgWindowInfoData {
  const processNameBytes = buf.slice(60, 315);
  const processNameEnd = processNameBytes.indexOf(0);
  const processName = processNameBytes.slice(0, processNameEnd === -1 ? 255 : processNameEnd).toString('utf8');
  
  return {
    windowId: buf.readUInt32LE(0),
    x: buf.readInt32LE(4),
    y: buf.readInt32LE(8),
    width: buf.readUInt32LE(12),
    height: buf.readUInt32LE(16),
    visible: buf.readUInt8(20) !== 0,
    opacity: buf.readFloatLE(24),
    scaleX: buf.readFloatLE(28),
    scaleY: buf.readFloatLE(32),
    rotation: buf.readFloatLE(36),
    layer: buf.readInt32LE(40),
    parentId: buf.readUInt32LE(44),
    state: buf.readUInt32LE(48),
    focused: buf.readUInt32LE(52) !== 0,
    pid: buf.readUInt32LE(56),
    processName,
    title: processName // Alias for convenience
  };
}

export function deserializeWindowAttributesData(buf: Buffer): IcmMsgWindowAttributesData {
  return {
    windowId: buf.readUInt32LE(0),
    visible: buf.readUInt32LE(4) !== 0,
    opacity: buf.readFloatLE(8),
    scaleX: buf.readFloatLE(12),
    scaleY: buf.readFloatLE(16),
    rotation: buf.readFloatLE(20)
  };
}

export function deserializeWindowLayerData(buf: Buffer): IcmMsgWindowLayerData {
  return {
    windowId: buf.readUInt32LE(0),
    layer: buf.readInt32LE(4),
    parentId: buf.readUInt32LE(8)
  };
}

export function deserializeWindowStateData(buf: Buffer): IcmMsgWindowStateData {
  return {
    windowId: buf.readUInt32LE(0),
    state: buf.readUInt32LE(4),
    focused: buf.readUInt32LE(8) !== 0
  };
}

export function serializeQueryScreenDimensions(msg: IcmMsgQueryScreenDimensions): Buffer {
  return Buffer.alloc(0);
}

export function deserializeScreenDimensionsData(buf: Buffer): IcmMsgScreenDimensionsData {
  return {
    totalWidth: buf.readUInt32LE(0),
    totalHeight: buf.readUInt32LE(4),
    scale: buf.readFloatLE(8)
  };
}

export function serializeQueryMonitors(msg: IcmMsgQueryMonitors): Buffer {
  return Buffer.alloc(0);
}

export function deserializeMonitorsData(buf: Buffer): IcmMsgMonitorsData {
  const numMonitors = buf.readUInt32LE(0);
  const monitors: IcmMonitorInfo[] = [];
  
  let offset = 4;
  const MONITOR_INFO_SIZE = 66; // 8+8+4+4+4+4+1+1+32
  
  for (let i = 0; i < numMonitors; i++) {
    if (offset + MONITOR_INFO_SIZE > buf.length) break;
    
    // Extract monitor name (32-byte fixed string starting at offset 34)
    const nameBytes = buf.slice(offset + 34, offset + 66);
    const nameEnd = nameBytes.indexOf(0);
    const name = nameBytes.slice(0, nameEnd === -1 ? 32 : nameEnd).toString('utf8');
    
    monitors.push({
      x: buf.readInt32LE(offset + 0),
      y: buf.readInt32LE(offset + 4),
      width: buf.readUInt32LE(offset + 8),
      height: buf.readUInt32LE(offset + 12),
      physicalWidth: buf.readUInt32LE(offset + 16),
      physicalHeight: buf.readUInt32LE(offset + 20),
      refreshRate: buf.readUInt32LE(offset + 24),
      scale: buf.readFloatLE(offset + 28),
      enabled: buf.readUInt8(offset + 32) !== 0,
      primary: buf.readUInt8(offset + 33) !== 0,
      name
    });
    
    offset += MONITOR_INFO_SIZE;
  }
  
  return { monitors };
}

export const IcmWindowIdRoot = 0;