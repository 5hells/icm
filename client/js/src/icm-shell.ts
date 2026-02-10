import * as net from 'net';
import { EventEmitter } from 'events';
import * as fs from 'fs';
import { createCanvas, loadImage } from '@napi-rs/canvas';
import {
    IcmIpcMsgType,
    IcmMsgCreateBuffer,
    IcmMsgDestroyBuffer,
    IcmMsgDrawRect,
    IcmMsgRegisterPointerEvent,
    IcmMsgRegisterKeyboardEvent,
    IcmMsgPointerEvent,
    IcmMsgKeyboardEvent,
    IcmMsgUploadImage,
    IcmMsgDestroyImage,
    IcmMsgDrawUploadedImage,
    IcmMsgDrawText,
    IcmMsgSetWindowVisible,
    IcmMsgRegisterKeybind,
    IcmMsgUnregisterKeybind,
    IcmMsgKeybindEvent,
    IcmMsgWindowCreated,
    IcmMsgWindowDestroyed,
    IcmMsgRegisterClickRegion,
    IcmMsgUnregisterClickRegion,
    IcmMsgClickRegionEvent,
    IcmMsgRequestScreenCopy,
    IcmMsgScreenCopyData,
    IcmMsgRegisterGlobalPointerEvent,
    IcmMsgRegisterGlobalKeyboardEvent,
    IcmMsgRegisterGlobalCaptureMouse,
    IcmMsgRegisterGlobalCaptureKeyboard,
    IcmMsgUnregisterGlobalCaptureKeyboard,
    IcmMsgUnregisterGlobalCaptureMouse,
    IcmMsgSetWindowPosition,
    IcmMsgSetWindowSize,
    IcmMsgSetWindowOpacity,
    IcmMsgSetWindowBlur,
    IcmMsgSetScreenEffect,
    IcmMsgSetWindowEffect,
    IcmMsgSetWindowTransform,
    IcmMsgSetWindowLayer,
    IcmMsgRaiseWindow,
    IcmMsgLowerWindow,
    IcmMsgSetWindowParent,
    IcmMsgSetWindowTransform3D,
    IcmMsgSetWindowMatrix,
    IcmMsgAnimateWindow,
    IcmMsgStopAnimation,
    IcmMsgSetWindowState,
    IcmMsgFocusWindow,
    IcmMsgBlurWindow,
    IcmMsgQueryWindowPosition,
    IcmMsgQueryWindowSize,
    IcmMsgQueryWindowAttributes,
    IcmMsgQueryWindowLayer,
    IcmMsgQueryWindowState,
    IcmMsgWindowPositionData,
    IcmMsgWindowSizeData,
    IcmMsgWindowAttributesData,
    IcmMsgWindowLayerData,
    IcmMsgWindowStateData,
    IcmMsgQueryScreenDimensions,
    IcmMsgScreenDimensionsData,
    IcmMsgQueryMonitors,
    IcmMsgMonitorsData,
    IcmMonitorInfo,
    IcmMsgCompositorShutdown,
    createHeader,
    serializeMessage,
    serializeCreateBuffer,
    serializeDestroyBuffer,
    serializeDrawRect,
    serializeRegisterPointerEvent,
    serializeRegisterKeyboardEvent,
    serializeUploadImage,
    serializeDestroyImage,
    serializeDrawUploadedImage,
    serializeDrawText,
    serializeSetWindowVisible,
    serializeRegisterKeybind,
    serializeUnregisterKeybind,
    serializeRegisterClickRegion,
    serializeUnregisterClickRegion,
    serializeRequestScreenCopy,
    serializeRegisterGlobalPointerEvent,
    serializeRegisterGlobalKeyboardEvent,
    serializeRegisterGlobalCaptureMouse,
    serializeRegisterGlobalCaptureKeyboard,
    serializeUnregisterGlobalCaptureKeyboard,
    serializeUnregisterGlobalCaptureMouse,
    serializeSetWindowPosition,
    serializeSetWindowSize,
    serializeSetWindowOpacity,
    serializeSetWindowBlur,
    serializeSetScreenEffect,
    serializeSetWindowEffect,
    serializeSetWindowTransform,
    serializeSetWindowLayer,
    serializeRaiseWindow,
    serializeLowerWindow,
    serializeSetWindowParent,
    serializeSetWindowTransform3D,
    serializeSetWindowMatrix,
    serializeAnimateWindow,
    serializeStopAnimation,
    serializeSetWindowState,
    serializeFocusWindow,
    serializeBlurWindow,
    serializeQueryWindowPosition,
    serializeQueryWindowSize,
    serializeQueryWindowAttributes,
    serializeQueryWindowLayer,
    serializeQueryWindowState,
    deserializePointerEvent,
    deserializeKeyboardEvent,
    deserializeKeybindEvent,
    deserializeWindowCreated,
    deserializeWindowDestroyed,
    deserializeClickRegionEvent,
    deserializeScreenCopyData,
    deserializeWindowPositionData,
    deserializeWindowSizeData,
    deserializeWindowAttributesData,
    deserializeWindowLayerData,
    deserializeWindowStateData,
    serializeQueryScreenDimensions,
    deserializeScreenDimensionsData,
    serializeQueryMonitors,
    deserializeMonitorsData,
    serializeQueryWindowInfo,
    deserializeWindowInfoData
} from './protocol';

export interface WindowOptions {
    width: number;
    height: number;
    x?: number;
    y?: number;
    layer?: number;
    color?: number;
}

export interface DrawRectOptions {
    x: number;
    y: number;
    width: number;
    height: number;
    color: number;
}

export interface DrawImageOptions {
    imageId: number;
    x: number;
    y: number;
    width?: number;
    height?: number;
    srcX?: number;
    srcY?: number;
    srcWidth?: number;
    srcHeight?: number;
    alpha?: number;
}

export interface DrawTextOptions {
    x: number;
    y: number;
    text: string;
    color: number;
    fontSize?: number;
}

interface IcmShellEventMap {
    connect: [];
    close: [];
    error: [Error];
    pointer: [IcmMsgPointerEvent];
    keyboard: [IcmMsgKeyboardEvent];
    keybind: [IcmMsgKeybindEvent];
    windowCreated: [IcmMsgWindowCreated];
    windowDestroyed: [IcmMsgWindowDestroyed];
    clickRegion: [IcmMsgClickRegionEvent];
    screenCopy: [IcmMsgScreenCopyData];
    windowPosition: [IcmMsgWindowPositionData];
    windowSize: [IcmMsgWindowSizeData];
    windowAttributes: [IcmMsgWindowAttributesData];
    windowLayer: [IcmMsgWindowLayerData];
    windowState: [IcmMsgWindowStateData];
    screenDimensions: [IcmMsgScreenDimensionsData];
    monitors: [IcmMsgMonitorsData];
    click: [{ x: number; y: number; windowId: number; btn: 'left' | 'right' | 'middle'; state: 'down' | 'up' }];
    release: [{ x: number; y: number; windowId: number, btn: 'left' | 'right' | 'middle'; state: 'down' | 'up' }];
    shutdown: [];
    windowInfo: [{ x: number; y: number; width: number; height: number; visible: boolean; opacity: number; layer: number; state: number }];
}

export class IcmShell extends EventEmitter<IcmShellEventMap> {
    private socket: net.Socket;
    private buffer: Buffer = Buffer.alloc(0);
    private nextBufferId = 1;
    private nextImageId = 1;
    private windows: Map<number, any> = new Map();

    constructor(socketPath?: string) {
        super();
        if (!socketPath) {
            const runtimeDir = process.env.XDG_RUNTIME_DIR || '/tmp';
            socketPath = `${runtimeDir}/icm.sock`;
        }
        this.socket = net.createConnection(socketPath);

        this.socket.on('connect', () => {
            console.log('Connected to ICM shell server');
            this.emit('connect');
        });

        this.socket.on('data', (data) => {
            this.buffer = Buffer.concat([this.buffer, data]);
            this.processMessages();
        });

        this.socket.on('error', (err) => {
            console.error('Socket error:', err);
            this.emit('error', err);
        });

        this.socket.on('close', () => {
            console.log('Connection closed');
            this.emit('close');
        });
    }

    private sendMessage(type: number, payload: Buffer) {
        const header = createHeader(type, payload.length);
        const message = serializeMessage(header, payload);
        console.log(`Sending message type ${type}, length ${message.length}`);
        this.socket.write(message);
    }

    private processMessages() {
        while (this.buffer.length >= 16) { // Header is 16 bytes
            const length = this.buffer.readUInt32LE(0);
            if (this.buffer.length < length) {
                break;
            }

            const header = {
                length,
                type: this.buffer.readUInt16LE(4),
                flags: this.buffer.readUInt16LE(6),
                sequence: this.buffer.readUInt32LE(8),
                numFds: this.buffer.readInt32LE(12)
            };

            const payload = this.buffer.slice(16, length); // Payload starts after 16-byte header
            this.handleMessage(header, payload);

            this.buffer = this.buffer.slice(length);
        }
    }

    private handleMessage(header: any, payload: Buffer) {
        console.log(`Received message type ${header.type}, length ${header.length}`);
        switch (header.type) {
            case IcmIpcMsgType.POINTER_EVENT:
                const pevent = deserializePointerEvent(payload);
                this.handlePointerEvent(pevent);
                break;
            case IcmIpcMsgType.KEYBOARD_EVENT:
                const kevent = deserializeKeyboardEvent(payload);
                this.handleKeyboardEvent(kevent);
                break;
            case IcmIpcMsgType.KEYBIND_EVENT:
                const kbevent = deserializeKeybindEvent(payload);
                this.handleKeybindEvent(kbevent);
                break;
            case IcmIpcMsgType.WINDOW_CREATED:
                const wcevent = deserializeWindowCreated(payload);
                this.handleWindowCreated(wcevent);
                break;
            case IcmIpcMsgType.WINDOW_DESTROYED:
                const wdevent = deserializeWindowDestroyed(payload);
                this.handleWindowDestroyed(wdevent);
                break;
            case IcmIpcMsgType.CLICK_REGION_EVENT:
                const crevent = deserializeClickRegionEvent(payload);
                this.handleClickRegionEvent(crevent);
                break;
            case IcmIpcMsgType.SCREEN_COPY_DATA:
                const screvent = deserializeScreenCopyData(payload);
                this.handleScreenCopyData(screvent);
                break;
            case IcmIpcMsgType.WINDOW_POSITION_DATA:
                const posData = deserializeWindowPositionData(payload);
                this.emit('windowPosition', posData);
                break;
            case IcmIpcMsgType.WINDOW_SIZE_DATA:
                const sizeData = deserializeWindowSizeData(payload);
                this.emit('windowSize', sizeData);
                break;
            case IcmIpcMsgType.WINDOW_ATTRIBUTES_DATA:
                const attrData = deserializeWindowAttributesData(payload);
                this.emit('windowAttributes', attrData);
                break;
            case IcmIpcMsgType.WINDOW_LAYER_DATA:
                const layerData = deserializeWindowLayerData(payload);
                this.emit('windowLayer', layerData);
                break;
            case IcmIpcMsgType.WINDOW_STATE_DATA:
                const stateData = deserializeWindowStateData(payload);
                this.emit('windowState', stateData);
                break;
            case IcmIpcMsgType.SCREEN_DIMENSIONS_DATA:
                const screenData = deserializeScreenDimensionsData(payload);
                this.emit('screenDimensions', screenData);
                break;
            case IcmIpcMsgType.MONITORS_DATA:
                const monitorsData = deserializeMonitorsData(payload);
                this.emit('monitors', monitorsData);
                break;
            case IcmIpcMsgType.COMPOSITOR_SHUTDOWN:
                this.handleCompositorShutdown();
                break;
            case IcmIpcMsgType.WINDOW_INFO_DATA:
                const windowInfo = deserializeWindowInfoData(payload);
                this.emit('windowInfo', windowInfo);
                break;
            default:
                console.log('Unknown message type:', header.type);
        }
    }

    private handlePointerEvent(event: IcmMsgPointerEvent) {
        // Emit global pointer event if window_id is 0
        if (event.windowId === 0) {
            this.emit('globalPointerEvent' as any, event);
        } else {
            this.emit('pointer', event);
        }
        
        if (event.button === 0x110 && event.state === 1) { // BTN_LEFT pressed
            this.emit('click', { x: event.x, y: event.y, windowId: event.windowId, btn: 'left', state: 'down' });
        }
        if (event.button === 0x110 && event.state === 0) { // BTN_LEFT released
            this.emit('release', { x: event.x, y: event.y, windowId: event.windowId, btn: 'left', state: 'up' });
        }
        if (event.button === 0x111 && event.state === 1) { // BTN_RIGHT pressed
            this.emit('click', { x: event.x, y: event.y, windowId: event.windowId, btn: 'right', state: 'down' });
        }
        if (event.button === 0x111 && event.state === 0) { // BTN_RIGHT released
            this.emit('release', { x: event.x, y: event.y, windowId: event.windowId, btn: 'right', state: 'up' });
        }
        if (event.button === 0x112 && event.state === 1) { // BTN_MIDDLE pressed
            this.emit('click', { x: event.x, y: event.y, windowId: event.windowId, btn: 'middle', state: 'down' });
        }
        if (event.button === 0x112 && event.state === 0) { // BTN_MIDDLE released
            this.emit('release', { x: event.x, y: event.y, windowId: event.windowId, btn: 'middle', state: 'up' });
        }
    }

    private handleKeyboardEvent(event: IcmMsgKeyboardEvent) {
        this.emit('keyboard', event);
    }

    private handleKeybindEvent(event: IcmMsgKeybindEvent) {
        this.emit('keybind', event);
    }

    private handleWindowCreated(event: IcmMsgWindowCreated) {
        this.emit('windowCreated', event);
    }

    private handleWindowDestroyed(event: IcmMsgWindowDestroyed) {
        this.emit('windowDestroyed', event);
    }

    private handleClickRegionEvent(event: IcmMsgClickRegionEvent) {
        this.emit('clickRegion', event);
    }

    private handleScreenCopyData(event: IcmMsgScreenCopyData) {
        this.emit('screenCopy', event);
    }

    private handleCompositorShutdown() {
        console.log('Compositor is shutting down');
        this.emit('shutdown');
    }

    // Public API

    async getScreenDimensions(): Promise<IcmMsgScreenDimensionsData> {
        this.queryScreenDimensions();
        return await new Promise<IcmMsgScreenDimensionsData>((resolve) => {
            this.once('screenDimensions', (e: IcmMsgScreenDimensionsData) => {
                resolve(e);
            });
        });
    }

    async queryWindowInfo(windowId: number): Promise<{ x: number; y: number; width: number; height: number; visible: boolean; opacity: number; layer: number; state: number }> {
        this.sendMessage(IcmIpcMsgType.QUERY_WINDOW_INFO, serializeQueryWindowInfo({ windowId }));
        return await new Promise((resolve) => {
            this.once('windowInfo', (info) => {
                resolve(info);
            });
        });
    }

    createWindow(options: WindowOptions): number {
        const bufferId = this.nextBufferId++;
        const createBuf: IcmMsgCreateBuffer = {
            bufferId,
            width: options.width,
            height: options.height,
            format: 0x34325241, // ARGB
            usageFlags: 0
        };
        this.sendMessage(IcmIpcMsgType.CREATE_BUFFER, serializeCreateBuffer(createBuf));

        // Set initial position (default to 0,0)
        const x = options.x ?? 0;
        const y = options.y ?? 0;
        this.setWindowPosition(bufferId, x, y);

        // Set initial size (already set in CREATE_BUFFER, but ensure consistency)
        this.setWindowSize(bufferId, options.width, options.height);

        // Set layer if provided (default is 0)
        if (options.layer !== undefined) {
            this.setWindowLayer(bufferId, options.layer);
        }

        // Register for events
        const regPointer: IcmMsgRegisterPointerEvent = { windowId: bufferId };
        this.sendMessage(IcmIpcMsgType.REGISTER_POINTER_EVENT, serializeRegisterPointerEvent(regPointer));

        const regKeyboard: IcmMsgRegisterKeyboardEvent = { windowId: bufferId };
        this.sendMessage(IcmIpcMsgType.REGISTER_KEYBOARD_EVENT, serializeRegisterKeyboardEvent(regKeyboard));

        this.windows.set(bufferId, { width: options.width, height: options.height, x, y });
        return bufferId;
    }

    destroyWindow(windowId: number) {
        const destroyBuf: IcmMsgDestroyBuffer = { bufferId: windowId };
        this.sendMessage(IcmIpcMsgType.DESTROY_BUFFER, serializeDestroyBuffer(destroyBuf));
        this.windows.delete(windowId);
    }

    drawRect(windowId: number, options: DrawRectOptions) {
        const draw: IcmMsgDrawRect = {
            windowId,
            x: options.x,
            y: options.y,
            width: options.width,
            height: options.height,
            colorRgba: options.color
        };
        this.sendMessage(IcmIpcMsgType.DRAW_RECT, serializeDrawRect(draw));
    }

    /**
     * Begin a batch of rendering operations for more efficient updates
     */
    beginBatch() {
        const msg = {};
        this.sendMessage(IcmIpcMsgType.BATCH_BEGIN, Buffer.alloc(0));
    }

    /**
     * End a batch of rendering operations
     */
    endBatch() {
        const msg = {};
        this.sendMessage(IcmIpcMsgType.BATCH_END, Buffer.alloc(0));
    }

    /**
     * Render a window with multiple operations efficiently
     * @param windowId Window to render
     * @param operations Array of drawing operations
     */
    batchRender(windowId: number, operations: Array<() => void>) {
        this.beginBatch();
        for (const op of operations) {
            op();
        }
        this.endBatch();
    }

    uploadImage(data: Uint8Array, width: number, height: number, format: number = 0): number {
        const imageId = this.nextImageId++;
        const upload: IcmMsgUploadImage = {
            imageId,
            width,
            height,
            format,
            data
        };
        this.sendMessage(IcmIpcMsgType.UPLOAD_IMAGE, serializeUploadImage(upload));
        return imageId;
    }

    async uploadImageFromFile(filePath: string): Promise<number> {
        const data = fs.readFileSync(filePath);
        const image = await loadImage(data);
        const width = image.width;
        const height = image.height;
        let rawData = new Uint8Array(width * height * 4);
        const canvas = createCanvas(width, height);
        const ctx = canvas.getContext('2d');
        ctx.drawImage(image, 0, 0);
        const imageData = ctx.getImageData(0, 0, width, height);
        for (let i = 0; i < width * height * 4; i++) {
            rawData[i] = imageData.data[i];
        }
        return this.uploadImage(rawData, width, height);
    }

    destroyImage(imageId: number) {
        const destroy: IcmMsgDestroyImage = { imageId };
        this.sendMessage(IcmIpcMsgType.DESTROY_IMAGE, serializeDestroyImage(destroy));
    }

    drawImage(windowId: number, options: DrawImageOptions) {
        const draw: IcmMsgDrawUploadedImage = {
            windowId,
            imageId: options.imageId,
            x: options.x,
            y: options.y,
            width: options.width || 0,
            height: options.height || 0,
            srcX: options.srcX || 0,
            srcY: options.srcY || 0,
            srcWidth: options.srcWidth || 0,
            srcHeight: options.srcHeight || 0,
            alpha: options.alpha || 255
        };
        this.sendMessage(IcmIpcMsgType.DRAW_UPLOADED_IMAGE, serializeDrawUploadedImage(draw));
    }

    drawText(windowId: number, options: DrawTextOptions) {
        const draw: IcmMsgDrawText = {
            windowId,
            x: options.x,
            y: options.y,
            colorRgba: options.color,
            fontSize: options.fontSize || 12,
            text: options.text
        };
        this.sendMessage(IcmIpcMsgType.DRAW_TEXT, serializeDrawText(draw));
    }

    setWindowVisible(windowId: number, visible: boolean) {
        const setVis: IcmMsgSetWindowVisible = {
            windowId,
            visible
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_VISIBLE, serializeSetWindowVisible(setVis));
    }

    registerKeybind(keybindId: number, modifiers: number, keycode: number) {
        const reg: IcmMsgRegisterKeybind = {
            keybindId,
            modifiers,
            keycode
        };
        this.sendMessage(IcmIpcMsgType.REGISTER_KEYBIND, serializeRegisterKeybind(reg));
    }

    unregisterKeybind(keybindId: number) {
        const unreg: IcmMsgUnregisterKeybind = {
            keybindId
        };
        this.sendMessage(IcmIpcMsgType.UNREGISTER_KEYBIND, serializeUnregisterKeybind(unreg));
    }

    registerClickRegion(windowId: number, regionId: number, x: number, y: number, width: number, height: number) {
        const reg: IcmMsgRegisterClickRegion = {
            windowId,
            regionId,
            x,
            y,
            width,
            height
        };
        this.sendMessage(IcmIpcMsgType.REGISTER_CLICK_REGION, serializeRegisterClickRegion(reg));
    }

    unregisterClickRegion(regionId: number) {
        const unreg: IcmMsgUnregisterClickRegion = {
            regionId
        };
        this.sendMessage(IcmIpcMsgType.UNREGISTER_CLICK_REGION, serializeUnregisterClickRegion(unreg));
    }

    requestScreenCopy(requestId: number, x: number = 0, y: number = 0, width: number = 1920, height: number = 1080) {
        const req: IcmMsgRequestScreenCopy = {
            requestId,
            x,
            y,
            width,
            height
        };
        this.sendMessage(IcmIpcMsgType.REQUEST_SCREEN_COPY, serializeRequestScreenCopy(req));
    }

    registerGlobalPointerEvent() {
        const reg: IcmMsgRegisterGlobalPointerEvent = {};
        this.sendMessage(IcmIpcMsgType.REGISTER_GLOBAL_POINTER_EVENT, serializeRegisterGlobalPointerEvent(reg));
    }

    registerPointerEvent(windowId: number) {
        const reg: IcmMsgRegisterPointerEvent = { windowId };
        this.sendMessage(IcmIpcMsgType.REGISTER_POINTER_EVENT, serializeRegisterPointerEvent(reg));
    }

    registerKeyboardEvent(windowId: number) {
        const reg: IcmMsgRegisterKeyboardEvent = { windowId };
        this.sendMessage(IcmIpcMsgType.REGISTER_KEYBOARD_EVENT, serializeRegisterKeyboardEvent(reg));
    }

    registerGlobalKeyboardEvent() {
        const reg: IcmMsgRegisterGlobalKeyboardEvent = {};
        this.sendMessage(IcmIpcMsgType.REGISTER_GLOBAL_KEYBOARD_EVENT, serializeRegisterGlobalKeyboardEvent(reg));
    }

    registerGlobalCaptureMouse() {
        const reg: IcmMsgRegisterGlobalCaptureMouse = {};
        this.sendMessage(IcmIpcMsgType.REGISTER_GLOBAL_CAPTURE_MOUSE, serializeRegisterGlobalCaptureMouse(reg));
    }

    registerGlobalCaptureKeyboard() {
        const reg: IcmMsgRegisterGlobalCaptureKeyboard = {};
        this.sendMessage(IcmIpcMsgType.REGISTER_GLOBAL_CAPTURE_KEYBOARD, serializeRegisterGlobalCaptureKeyboard(reg));
    }

    unregisterGlobalCaptureKeyboard() {
        const unreg: IcmMsgUnregisterGlobalCaptureKeyboard = {};
        this.sendMessage(IcmIpcMsgType.UNREGISTER_GLOBAL_CAPTURE_KEYBOARD, serializeUnregisterGlobalCaptureKeyboard(unreg));
    }

    unregisterGlobalCaptureMouse() {
        const unreg: IcmMsgUnregisterGlobalCaptureMouse = {};
        this.sendMessage(IcmIpcMsgType.UNREGISTER_GLOBAL_CAPTURE_MOUSE, serializeUnregisterGlobalCaptureMouse(unreg));
    }

    setWindowPosition(windowId: number, x: number, y: number) {
        const setPos: IcmMsgSetWindowPosition = {
            windowId,
            x,
            y
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_POSITION, serializeSetWindowPosition(setPos));
    }

    setWindowSize(windowId: number, width: number, height: number) {
        const setSize: IcmMsgSetWindowSize = {
            windowId,
            width,
            height
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_SIZE, serializeSetWindowSize(setSize));
    }

    setWindowOpacity(windowId: number, opacity: number) {
        const setOpacity: IcmMsgSetWindowOpacity = {
            windowId,
            opacity
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_OPACITY, serializeSetWindowOpacity(setOpacity));
    }

    setWindowBlur(windowId: number, blurRadius: number, enabled: boolean) {
        const setBlur: IcmMsgSetWindowBlur = {
            windowId,
            blurRadius,
            enabled
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_BLUR, serializeSetWindowBlur(setBlur));
    }

    setScreenEffect(equation: string | { build(): string }, enabled: boolean) {
        const equationStr = typeof equation === 'string' ? equation : equation.build();
        const setEffect: IcmMsgSetScreenEffect = {
            equation: equationStr,
            enabled
        };
        this.sendMessage(IcmIpcMsgType.SET_SCREEN_EFFECT, serializeSetScreenEffect(setEffect));
    }

    setWindowEffect(windowId: number, equation: string | { build(): string }, enabled: boolean) {
        const equationStr = typeof equation === 'string' ? equation : equation.build();
        const setEffect: IcmMsgSetWindowEffect = {
            windowId,
            equation: equationStr,
            enabled
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_EFFECT, serializeSetWindowEffect(setEffect));
    }

    setWindowTransform(windowId: number, scaleX: number, scaleY: number, rotation: number) {
        const setTransform: IcmMsgSetWindowTransform = {
            windowId,
            scaleX,
            scaleY,
            rotation
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_TRANSFORM, serializeSetWindowTransform(setTransform));
    }

    setWindowLayer(windowId: number, layer: number) {
        const setLayer: IcmMsgSetWindowLayer = {
            windowId,
            layer
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_LAYER, serializeSetWindowLayer(setLayer));
    }

    raiseWindow(windowId: number) {
        const raise: IcmMsgRaiseWindow = { windowId };
        this.sendMessage(IcmIpcMsgType.RAISE_WINDOW, serializeRaiseWindow(raise));
    }

    lowerWindow(windowId: number) {
        const lower: IcmMsgLowerWindow = { windowId };
        this.sendMessage(IcmIpcMsgType.LOWER_WINDOW, serializeLowerWindow(lower));
    }

    setWindowParent(windowId: number, parentId: number) {
        const setParent: IcmMsgSetWindowParent = {
            windowId,
            parentId
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_PARENT, serializeSetWindowParent(setParent));
    }

    setWindowTransform3D(windowId: number, translateX: number, translateY: number, translateZ: number,
                        rotateX: number, rotateY: number, rotateZ: number,
                        scaleX: number, scaleY: number, scaleZ: number) {
        const setTransform3D: IcmMsgSetWindowTransform3D = {
            windowId,
            translateX, translateY, translateZ,
            rotateX, rotateY, rotateZ,
            scaleX, scaleY, scaleZ
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_TRANSFORM_3D, serializeSetWindowTransform3D(setTransform3D));
    }

    setWindowMatrix(windowId: number, matrix: number[]) {
        const setMatrix: IcmMsgSetWindowMatrix = {
            windowId,
            matrix
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_MATRIX, serializeSetWindowMatrix(setMatrix));
    }

    animateWindow(windowId: number, durationMs: number, options: {
        targetX?: number;
        targetY?: number;
        targetScaleX?: number;
        targetScaleY?: number;
        targetOpacity?: number;
        targetTranslateX?: number;
        targetTranslateY?: number;
        targetTranslateZ?: number;
        targetRotateX?: number;
        targetRotateY?: number;
        targetRotateZ?: number;
        targetScaleZ?: number;
    }) {
        let flags = 0;
        if (options.targetX !== undefined || options.targetY !== undefined) flags |= 1; // position
        if (options.targetScaleX !== undefined || options.targetScaleY !== undefined) flags |= 2; // scale
        if (options.targetOpacity !== undefined) flags |= 4; // opacity
        if (options.targetTranslateX !== undefined || options.targetTranslateY !== undefined || options.targetTranslateZ !== undefined) flags |= 8; // 3d translate
        if (options.targetRotateX !== undefined || options.targetRotateY !== undefined || options.targetRotateZ !== undefined) flags |= 16; // 3d rotate
        if (options.targetScaleZ !== undefined) flags |= 32; // 3d scale

        const animate: IcmMsgAnimateWindow = {
            windowId,
            durationMs,
            targetX: options.targetX ?? 0,
            targetY: options.targetY ?? 0,
            targetScaleX: options.targetScaleX ?? 1,
            targetScaleY: options.targetScaleY ?? 1,
            targetOpacity: options.targetOpacity ?? 1,
            targetTranslateX: options.targetTranslateX ?? 0,
            targetTranslateY: options.targetTranslateY ?? 0,
            targetTranslateZ: options.targetTranslateZ ?? 0,
            targetRotateX: options.targetRotateX ?? 0,
            targetRotateY: options.targetRotateY ?? 0,
            targetRotateZ: options.targetRotateZ ?? 0,
            targetScaleZ: options.targetScaleZ ?? 1,
            flags
        };
        this.sendMessage(IcmIpcMsgType.ANIMATE_WINDOW, serializeAnimateWindow(animate));
    }

    stopAnimation(windowId: number) {
        const stop: IcmMsgStopAnimation = { windowId };
        this.sendMessage(IcmIpcMsgType.STOP_ANIMATION, serializeStopAnimation(stop));
    }

    setWindowState(windowId: number, state: number) {
        const setState: IcmMsgSetWindowState = {
            windowId,
            state
        };
        this.sendMessage(IcmIpcMsgType.SET_WINDOW_STATE, serializeSetWindowState(setState));
    }

    focusWindow(windowId: number) {
        const focus: IcmMsgFocusWindow = { windowId };
        this.sendMessage(IcmIpcMsgType.FOCUS_WINDOW, serializeFocusWindow(focus));
    }

    blurWindow(windowId: number) {
        const blur: IcmMsgBlurWindow = { windowId };
        this.sendMessage(IcmIpcMsgType.BLUR_WINDOW, serializeBlurWindow(blur));
    }

    queryWindowPosition(windowId: number) {
        const query: IcmMsgQueryWindowPosition = { windowId };
        this.sendMessage(IcmIpcMsgType.QUERY_WINDOW_POSITION, serializeQueryWindowPosition(query));
    }

    queryWindowSize(windowId: number) {
        const query: IcmMsgQueryWindowSize = { windowId };
        this.sendMessage(IcmIpcMsgType.QUERY_WINDOW_SIZE, serializeQueryWindowSize(query));
    }

    queryWindowAttributes(windowId: number) {
        const query: IcmMsgQueryWindowAttributes = { windowId };
        this.sendMessage(IcmIpcMsgType.QUERY_WINDOW_ATTRIBUTES, serializeQueryWindowAttributes(query));
    }

    queryWindowLayer(windowId: number) {
        const query: IcmMsgQueryWindowLayer = { windowId };
        this.sendMessage(IcmIpcMsgType.QUERY_WINDOW_LAYER, serializeQueryWindowLayer(query));
    }

    queryWindowState(windowId: number) {
        const query: IcmMsgQueryWindowState = { windowId };
        this.sendMessage(IcmIpcMsgType.QUERY_WINDOW_STATE, serializeQueryWindowState(query));
    }

    queryScreenDimensions() {
        const query: IcmMsgQueryScreenDimensions = {};
        this.sendMessage(IcmIpcMsgType.QUERY_SCREEN_DIMENSIONS, serializeQueryScreenDimensions(query));
    }

    queryMonitors() {
        const query: IcmMsgQueryMonitors = {};
        this.sendMessage(IcmIpcMsgType.QUERY_MONITORS, serializeQueryMonitors(query));
    }

    close() {
        this.socket.end();
    }
}

export class ClickableRenderer {
    private shell: IcmShell;
    private windowId: number;
    private regions: Map<number, { x: number; y: number; width: number; height: number }> = new Map();

    constructor(shell: IcmShell, windowId: number) {
        this.shell = shell;
        this.windowId = windowId;

        this.shell.on('clickRegion', (event) => {
            const region = this.regions.get(event.regionId);
            if (region) {
                const buttonMap: { [key: number]: 'left' | 'right' | 'middle' } = {
                    0x110: 'left',
                    0x111: 'right',
                    0x112: 'middle'
                };
                const clickEvent = {
                    x: this.regions.get(event.regionId)!.x,
                    y: this.regions.get(event.regionId)!.y,
                    windowId: this.windowId,
                    btn: buttonMap[event.button] || 'left',
                    state: event.state === 1 ? 'down' : 'up' as 'down' | 'up'
                };
                this.shell.emit('click', clickEvent);
            }
        });
    }

    addClickableRegion(regionId: number, x: number, y: number, width: number, height: number) {
        this.regions.set(regionId, { x, y, width, height });
        this.shell.registerClickRegion(this.windowId, regionId, x, y, width, height);
    }

    removeClickableRegion(regionId: number) {
        this.regions.delete(regionId);
        this.shell.unregisterClickRegion(regionId);
    }
}

export enum Modifier {
    Shift = 1 << 0,
    Ctrl = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3
}

export class KeybindManager {
    private shell: IcmShell;
    private keybinds: Map<number, { modifiers: number; keycode: number }> = new Map();

    constructor(shell: IcmShell) {
        this.shell = shell;

        this.shell.on('keybind', (event) => {
            const keybind = this.keybinds.get(event.keybindId);
            if (keybind) {
                this.shell.emit('keybind', event);
            }
        });
    }

    registerKeybind(keybindId: number, modifiers: number, keycode: number) {
        this.keybinds.set(keybindId, { modifiers, keycode });
        this.shell.registerKeybind(keybindId, modifiers, keycode);
    }

    unregisterKeybind(keybindId: number) {
        this.keybinds.delete(keybindId);
        this.shell.unregisterKeybind(keybindId);
    }
}

export class KeyboardCapturingRenderer {
    private shell: IcmShell;
    private windowId: number;

    constructor(shell: IcmShell, windowId: number) {
        this.shell = shell;
        this.windowId = windowId;

        this.shell.on('keyboard', (event) => {
            if (event.windowId === this.windowId) {
                this.shell.emit('keyboard', event);
            }
        });
    }

    captureKeyboard() {
        this.shell.registerGlobalCaptureKeyboard();
    }

    releaseKeyboard() {
    }
}

export class ICMWindow {
    private shell: IcmShell;
    private windowId: number;

    constructor(shell: IcmShell, options: WindowOptions) {
        this.shell = shell;
        this.windowId = this.shell.createWindow(options);
    }

    drawRect(options: DrawRectOptions) {
        this.shell.drawRect(this.windowId, options);
    }

    uploadImage(data: Uint8Array, width: number, height: number, format: number = 0): number {
        return this.shell.uploadImage(data, width, height, format);
    }

    async uploadImageFromFile(filePath: string): Promise<number> {
        return await this.shell.uploadImageFromFile(filePath);
    }

    drawImage(options: DrawImageOptions) {
        this.shell.drawImage(this.windowId, options);
    }

    drawText(options: DrawTextOptions) {
        this.shell.drawText(this.windowId, options);
    }

    setVisible(visible: boolean) {
        this.shell.setWindowVisible(this.windowId, visible);
    }

    setPosition(x: number, y: number) {
        this.shell.setWindowPosition(this.windowId, x, y);
    }

    setSize(width: number, height: number) {
        this.shell.setWindowSize(this.windowId, width, height);
    }

    setOpacity(opacity: number) {
        this.shell.setWindowOpacity(this.windowId, opacity);
    }

    setTransform(scaleX: number, scaleY: number, rotation: number) {
        this.shell.setWindowTransform(this.windowId, scaleX, scaleY, rotation);
    }

    /**
     * Render multiple operations as a batch for better performance
     * @param operations Array of drawing operation functions
     */
    batchRender(operations: Array<() => void>) {
        this.shell.beginBatch();
        for (const op of operations) {
            op();
        }
        this.shell.endBatch();
    }

    /**
     * Re-render the window (refresh/update)
     */
    update() {
        // Schedule a frame update (can be expensive, so group with other updates)
        this.shell.endBatch();
        this.shell.beginBatch();
    }

    /**
     * Clear the window by drawing a transparent rectangle
     */
    clear() {
        this.drawRect({
            x: 0,
            y: 0,
            width: 9999,
            height: 9999,
            color: 0x00000000 // Transparent black
        });
    }

    /**
     * Focus this window (bring to front and give keyboard focus)
     */
    focus() {
        this.shell.focusWindow(this.windowId);
    }

    /**
     * Blur this window (remove focus)
     */
    blur() {
        this.shell.blurWindow(this.windowId);
    }

    async queryPosition() {
        this.shell.queryWindowPosition(this.windowId);
        return new Promise<{ x: number; y: number }>((resolve) => {
            this.shell.once('windowPosition', (event) => {
                if (event.windowId === this.windowId) {
                    resolve({ x: event.x, y: event.y });
                }
            });
        });
    }

    async querySize() {
        this.shell.queryWindowSize(this.windowId);
        return new Promise<{ width: number; height: number }>((resolve) => {
            this.shell.once('windowSize', (event) => {
                if (event.windowId === this.windowId) {
                    resolve({ width: event.width, height: event.height });
                }
            });
        });
    }

    async queryAttributes() {
        this.shell.queryWindowAttributes(this.windowId);
        return new Promise<{ visible: boolean; opacity: number }>((resolve) => {
            this.shell.once('windowAttributes', (event) => {
                if (event.windowId === this.windowId) {
                    resolve({ visible: event.visible, opacity: event.opacity });
                }
            });
        });
    }

    destroy() {
        this.shell.destroyWindow(this.windowId);
    }
}

export { PixelEffectBuilder, createPixelEffect, ShapeBuilder, createShape, ShapeSpec } from './pixel-effect-builder';