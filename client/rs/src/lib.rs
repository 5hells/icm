use std::io::{Read, Write};
use std::os::unix::net::UnixStream;
use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};

pub const ICM_IPC_VERSION: u32 = 2;
pub const ICM_MAX_FDS_PER_MSG: usize = 4;

#[derive(Debug, Clone, Copy)]
#[repr(u16)]
pub enum IcmIpcMsgType {
    CreateWindow = 1,
    DestroyWindow = 2,
    SetWindow = 3,
    SetLayer = 4,
    SetAttachments = 5,
    DrawRect = 6,
    ClearRects = 7,
    ImportDmabuf = 8,
    ExportDmabuf = 9,
    DrawLine = 10,
    DrawCircle = 11,
    DrawPolygon = 12,
    DrawImage = 13,
    BlitBuffer = 14,
    BatchBegin = 15,
    BatchEnd = 16,
    ExportSurface = 17,
    ImportSurface = 18,
    CreateBuffer = 19,
    DestroyBuffer = 20,
    QueryBufferInfo = 21,
    RegisterPointerEvent = 22,
    RegisterKeyboardEvent = 23,
    QueryCaptureMouse = 24,
    QueryCaptureKeyboard = 25,
    PointerEvent = 26,
    KeyboardEvent = 27,
    UploadImage = 28,
    DestroyImage = 29,
    DrawUploadedImage = 30,
    DrawText = 31,
    SetWindowVisible = 32,
    RegisterKeybind = 33,
    UnregisterKeybind = 34,
    KeybindEvent = 35,
    WindowCreated = 36,
    WindowDestroyed = 37,
    RegisterClickRegion = 38,
    UnregisterClickRegion = 39,
    ClickRegionEvent = 40,
    RequestScreenCopy = 41,
    ScreenCopyData = 42,
    RegisterGlobalPointerEvent = 43,
    RegisterGlobalKeyboardEvent = 44,
    RegisterGlobalCaptureMouse = 45,
    RegisterGlobalCaptureKeyboard = 46,
    UnregisterGlobalCaptureKeyboard = 58,
    UnregisterGlobalCaptureMouse = 59,
    SetWindowPosition = 47,
    SetWindowSize = 48,
    SetWindowOpacity = 49,
    SetWindowTransform = 50,
    SetWindowBlur = 78,
    SetScreenEffect = 79,
    SetWindowEffect = 80,
    SetWindowLayer = 60,
    RaiseWindow = 61,
    LowerWindow = 62,
    SetWindowParent = 63,
    SetWindowTransform3d = 64,
    SetWindowMatrix = 65,
    SetWindowState = 66,
    FocusWindow = 67,
    BlurWindow = 83,
    QueryWindowPosition = 52,
    QueryWindowSize = 53,
    QueryWindowAttributes = 54,
    QueryWindowLayer = 68,
    QueryWindowState = 69,
    WindowPositionData = 55,
    WindowSizeData = 56,
    WindowAttributesData = 57,
    WindowLayerData = 70,
    WindowStateData = 71,
    QueryScreenDimensions = 72,
    ScreenDimensionsData = 73,
    QueryMonitors = 74,
    MonitorsData = 75,
    CompositorShutdown = 51,
    QueryWindowInfo = 76,
    WindowInfoData = 77,
    AnimateWindow = 81,
    StopAnimation = 82,
    SetWindowMeshTransform = 84,
    ClearWindowMeshTransform = 85,
    UpdateWindowMeshVertices = 86,
    QueryToplevelWindows = 87,
    ToplevelWindowsData = 88,
    SubscribeWindowEvents = 89,
    UnsubscribeWindowEvents = 90,
    WindowTitleChanged = 91,
    WindowStateChanged = 92,
    LaunchApp = 93,
}

#[derive(Debug, Clone)]
pub struct IcmIpcHeader {
    pub length: u32,
    pub msg_type: u16,
    pub flags: u16,
    pub sequence: u32,
    pub num_fds: i32,
}

impl IcmIpcHeader {
    pub fn new(msg_type: IcmIpcMsgType, payload_size: usize) -> Self {
        Self {
            length: (16 + payload_size) as u32,
            msg_type: msg_type as u16,
            flags: 0,
            sequence: 0,
            num_fds: 0,
        }
    }

    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(16);
        buf.write_u32::<LittleEndian>(self.length).unwrap();
        buf.write_u16::<LittleEndian>(self.msg_type).unwrap();
        buf.write_u16::<LittleEndian>(self.flags).unwrap();
        buf.write_u32::<LittleEndian>(self.sequence).unwrap();
        buf.write_i32::<LittleEndian>(self.num_fds).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            length: reader.read_u32::<LittleEndian>()?,
            msg_type: reader.read_u16::<LittleEndian>()?,
            flags: reader.read_u16::<LittleEndian>()?,
            sequence: reader.read_u32::<LittleEndian>()?,
            num_fds: reader.read_i32::<LittleEndian>()?,
        })
    }
}

// Message structures
#[derive(Debug, Clone)]
pub struct IcmMsgCreateBuffer {
    pub buffer_id: u32,
    pub width: u32,
    pub height: u32,
    pub format: u32,
    pub usage_flags: u32,
}

impl IcmMsgCreateBuffer {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(20);
        buf.write_u32::<LittleEndian>(self.buffer_id).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf.write_u32::<LittleEndian>(self.format).unwrap();
        buf.write_u32::<LittleEndian>(self.usage_flags).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            buffer_id: reader.read_u32::<LittleEndian>()?,
            width: reader.read_u32::<LittleEndian>()?,
            height: reader.read_u32::<LittleEndian>()?,
            format: reader.read_u32::<LittleEndian>()?,
            usage_flags: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgDestroyBuffer {
    pub buffer_id: u32,
}

impl IcmMsgDestroyBuffer {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.buffer_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            buffer_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgCreateWindow {
    pub window_id: u32,
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub layer: u32,
    pub color_rgba: u32,
}

impl IcmMsgCreateWindow {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(24);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_i32::<LittleEndian>(self.x).unwrap();
        buf.write_i32::<LittleEndian>(self.y).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf.write_u32::<LittleEndian>(self.layer).unwrap();
        buf.write_u32::<LittleEndian>(self.color_rgba).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            x: reader.read_i32::<LittleEndian>()?,
            y: reader.read_i32::<LittleEndian>()?,
            width: reader.read_u32::<LittleEndian>()?,
            height: reader.read_u32::<LittleEndian>()?,
            layer: reader.read_u32::<LittleEndian>()?,
            color_rgba: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgDrawRect {
    pub window_id: u32,
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub color_rgba: u32,
}

impl IcmMsgDrawRect {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(24);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_i32::<LittleEndian>(self.x).unwrap();
        buf.write_i32::<LittleEndian>(self.y).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf.write_u32::<LittleEndian>(self.color_rgba).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            x: reader.read_i32::<LittleEndian>()?,
            y: reader.read_i32::<LittleEndian>()?,
            width: reader.read_u32::<LittleEndian>()?,
            height: reader.read_u32::<LittleEndian>()?,
            color_rgba: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgRegisterPointerEvent {
    pub window_id: u32,
}

impl IcmMsgRegisterPointerEvent {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgRegisterKeyboardEvent {
    pub window_id: u32,
}

impl IcmMsgRegisterKeyboardEvent {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryCaptureMouse {
    pub window_id: u32,
}

impl IcmMsgQueryCaptureMouse {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryCaptureKeyboard {
    pub window_id: u32,
}

impl IcmMsgQueryCaptureKeyboard {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgPointerEvent {
    pub window_id: u32,
    pub time: u32,
    pub button: u32,
    pub state: u32,
    pub x: i32,
    pub y: i32,
}

impl IcmMsgPointerEvent {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(24);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.time).unwrap();
        buf.write_u32::<LittleEndian>(self.button).unwrap();
        buf.write_u32::<LittleEndian>(self.state).unwrap();
        buf.write_i32::<LittleEndian>(self.x).unwrap();
        buf.write_i32::<LittleEndian>(self.y).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            time: reader.read_u32::<LittleEndian>()?,
            button: reader.read_u32::<LittleEndian>()?,
            state: reader.read_u32::<LittleEndian>()?,
            x: reader.read_i32::<LittleEndian>()?,
            y: reader.read_i32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgKeyboardEvent {
    pub window_id: u32,
    pub time: u32,
    pub keycode: u32,
    pub state: u32,
    pub modifiers: u32,
}

impl IcmMsgKeyboardEvent {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(20);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.time).unwrap();
        buf.write_u32::<LittleEndian>(self.keycode).unwrap();
        buf.write_u32::<LittleEndian>(self.state).unwrap();
        buf.write_u32::<LittleEndian>(self.modifiers).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            time: reader.read_u32::<LittleEndian>()?,
            keycode: reader.read_u32::<LittleEndian>()?,
            state: reader.read_u32::<LittleEndian>()?,
            modifiers: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgUploadImage {
    pub image_id: u32,
    pub width: u32,
    pub height: u32,
    pub format: u32,
    pub data: Vec<u8>,
}

impl IcmMsgUploadImage {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(20 + self.data.len());
        buf.write_u32::<LittleEndian>(self.image_id).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf.write_u32::<LittleEndian>(self.format).unwrap();
        buf.write_u32::<LittleEndian>(self.data.len() as u32).unwrap();
        buf.extend_from_slice(&self.data);
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        let image_id = reader.read_u32::<LittleEndian>()?;
        let width = reader.read_u32::<LittleEndian>()?;
        let height = reader.read_u32::<LittleEndian>()?;
        let format = reader.read_u32::<LittleEndian>()?;
        let data_size = reader.read_u32::<LittleEndian>()? as usize;
        let mut data = vec![0u8; data_size];
        reader.read_exact(&mut data)?;
        Ok(Self {
            image_id,
            width,
            height,
            format,
            data,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgDestroyImage {
    pub image_id: u32,
}

impl IcmMsgDestroyImage {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.image_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            image_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgDrawUploadedImage {
    pub window_id: u32,
    pub image_id: u32,
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub src_x: u32,
    pub src_y: u32,
    pub src_width: u32,
    pub src_height: u32,
    pub alpha: u8,
}

impl IcmMsgDrawUploadedImage {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(32);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.image_id).unwrap();
        buf.write_i32::<LittleEndian>(self.x).unwrap();
        buf.write_i32::<LittleEndian>(self.y).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf.write_u32::<LittleEndian>(self.src_x).unwrap();
        buf.write_u32::<LittleEndian>(self.src_y).unwrap();
        buf.write_u32::<LittleEndian>(self.src_width).unwrap();
        buf.write_u32::<LittleEndian>(self.src_height).unwrap();
        buf.write_u8(self.alpha).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            image_id: reader.read_u32::<LittleEndian>()?,
            x: reader.read_i32::<LittleEndian>()?,
            y: reader.read_i32::<LittleEndian>()?,
            width: reader.read_u32::<LittleEndian>()?,
            height: reader.read_u32::<LittleEndian>()?,
            src_x: reader.read_u32::<LittleEndian>()?,
            src_y: reader.read_u32::<LittleEndian>()?,
            src_width: reader.read_u32::<LittleEndian>()?,
            src_height: reader.read_u32::<LittleEndian>()?,
            alpha: reader.read_u8()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgDrawText {
    pub window_id: u32,
    pub x: i32,
    pub y: i32,
    pub color_rgba: u32,
    pub font_size: u32,
    pub text: String,
}

impl IcmMsgDrawText {
    pub fn serialize(&self) -> Vec<u8> {
        let text_bytes = self.text.as_bytes();
        let mut buf = Vec::with_capacity(20 + text_bytes.len() + 1);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_i32::<LittleEndian>(self.x).unwrap();
        buf.write_i32::<LittleEndian>(self.y).unwrap();
        buf.write_u32::<LittleEndian>(self.color_rgba).unwrap();
        buf.write_u32::<LittleEndian>(self.font_size).unwrap();
        buf.extend_from_slice(text_bytes);
        buf.push(0); // null terminator
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        let window_id = reader.read_u32::<LittleEndian>()?;
        let x = reader.read_i32::<LittleEndian>()?;
        let y = reader.read_i32::<LittleEndian>()?;
        let color_rgba = reader.read_u32::<LittleEndian>()?;
        let font_size = reader.read_u32::<LittleEndian>()?;
        let mut text_bytes = Vec::new();
        let mut byte = [0u8];
        loop {
            reader.read_exact(&mut byte)?;
            if byte[0] == 0 {
                break;
            }
            text_bytes.push(byte[0]);
        }
        let text = String::from_utf8(text_bytes).map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e))?;
        Ok(Self {
            window_id,
            x,
            y,
            color_rgba,
            font_size,
            text,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowVisible {
    pub window_id: u32,
    pub visible: bool,
}

impl IcmMsgSetWindowVisible {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(5);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u8(self.visible as u8).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            visible: reader.read_u8()? != 0,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgRegisterKeybind {
    pub keybind_id: u32,
    pub modifiers: u32,
    pub keycode: u32,
}

impl IcmMsgRegisterKeybind {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(12);
        buf.write_u32::<LittleEndian>(self.keybind_id).unwrap();
        buf.write_u32::<LittleEndian>(self.modifiers).unwrap();
        buf.write_u32::<LittleEndian>(self.keycode).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            keybind_id: reader.read_u32::<LittleEndian>()?,
            modifiers: reader.read_u32::<LittleEndian>()?,
            keycode: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgUnregisterKeybind {
    pub keybind_id: u32,
}

impl IcmMsgUnregisterKeybind {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.keybind_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            keybind_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgKeybindEvent {
    pub keybind_id: u32,
}

impl IcmMsgKeybindEvent {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.keybind_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            keybind_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgWindowCreated {
    pub window_id: u32,
    pub width: u32,
    pub height: u32,
    pub decorated: bool,
    pub focused: bool,
}

impl IcmMsgWindowCreated {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(12);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf.write_u8(self.decorated as u8).unwrap();
        buf.write_u8(self.focused as u8).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            width: reader.read_u32::<LittleEndian>()?,
            height: reader.read_u32::<LittleEndian>()?,
            decorated: reader.read_u8()? != 0,
            focused: reader.read_u8()? != 0,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgWindowDestroyed {
    pub window_id: u32,
}

impl IcmMsgWindowDestroyed {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgRegisterClickRegion {
    pub window_id: u32,
    pub region_id: u32,
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
}

impl IcmMsgRegisterClickRegion {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(20);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.region_id).unwrap();
        buf.write_i32::<LittleEndian>(self.x).unwrap();
        buf.write_i32::<LittleEndian>(self.y).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            region_id: reader.read_u32::<LittleEndian>()?,
            x: reader.read_i32::<LittleEndian>()?,
            y: reader.read_i32::<LittleEndian>()?,
            width: reader.read_u32::<LittleEndian>()?,
            height: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgUnregisterClickRegion {
    pub region_id: u32,
}

impl IcmMsgUnregisterClickRegion {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.region_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            region_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgClickRegionEvent {
    pub region_id: u32,
    pub button: u32,
    pub state: u32,
}

impl IcmMsgClickRegionEvent {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(12);
        buf.write_u32::<LittleEndian>(self.region_id).unwrap();
        buf.write_u32::<LittleEndian>(self.button).unwrap();
        buf.write_u32::<LittleEndian>(self.state).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            region_id: reader.read_u32::<LittleEndian>()?,
            button: reader.read_u32::<LittleEndian>()?,
            state: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgRequestScreenCopy {
    pub request_id: u32,
    pub x: u32,
    pub y: u32,
    pub width: u32,
    pub height: u32,
}

impl IcmMsgRequestScreenCopy {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(20);
        buf.write_u32::<LittleEndian>(self.request_id).unwrap();
        buf.write_u32::<LittleEndian>(self.x).unwrap();
        buf.write_u32::<LittleEndian>(self.y).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            request_id: reader.read_u32::<LittleEndian>()?,
            x: reader.read_u32::<LittleEndian>()?,
            y: reader.read_u32::<LittleEndian>()?,
            width: reader.read_u32::<LittleEndian>()?,
            height: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgScreenCopyData {
    pub request_id: u32,
    pub width: u32,
    pub height: u32,
    pub format: u32,
    pub data: Vec<u8>,
}

impl IcmMsgScreenCopyData {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(16 + self.data.len());
        buf.write_u32::<LittleEndian>(self.request_id).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf.write_u32::<LittleEndian>(self.format).unwrap();
        buf.write_u32::<LittleEndian>(self.data.len() as u32).unwrap();
        buf.extend_from_slice(&self.data);
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        let request_id = reader.read_u32::<LittleEndian>()?;
        let width = reader.read_u32::<LittleEndian>()?;
        let height = reader.read_u32::<LittleEndian>()?;
        let format = reader.read_u32::<LittleEndian>()?;
        let data_size = reader.read_u32::<LittleEndian>()? as usize;
        let mut data = vec![0u8; data_size];
        reader.read_exact(&mut data)?;
        Ok(Self {
            request_id,
            width,
            height,
            format,
            data,
        })
    }
}

// Global event messages (no payload)
#[derive(Debug, Clone)]
pub struct IcmMsgRegisterGlobalPointerEvent;

impl IcmMsgRegisterGlobalPointerEvent {
    pub fn serialize(&self) -> Vec<u8> {
        Vec::new()
    }

    pub fn deserialize<R: Read>(_reader: &mut R) -> std::io::Result<Self> {
        Ok(Self)
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgRegisterGlobalKeyboardEvent;

impl IcmMsgRegisterGlobalKeyboardEvent {
    pub fn serialize(&self) -> Vec<u8> {
        Vec::new()
    }

    pub fn deserialize<R: Read>(_reader: &mut R) -> std::io::Result<Self> {
        Ok(Self)
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgRegisterGlobalCaptureMouse;

impl IcmMsgRegisterGlobalCaptureMouse {
    pub fn serialize(&self) -> Vec<u8> {
        Vec::new()
    }

    pub fn deserialize<R: Read>(_reader: &mut R) -> std::io::Result<Self> {
        Ok(Self)
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgRegisterGlobalCaptureKeyboard;

impl IcmMsgRegisterGlobalCaptureKeyboard {
    pub fn serialize(&self) -> Vec<u8> {
        Vec::new()
    }

    pub fn deserialize<R: Read>(_reader: &mut R) -> std::io::Result<Self> {
        Ok(Self)
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgUnregisterGlobalCaptureKeyboard;

impl IcmMsgUnregisterGlobalCaptureKeyboard {
    pub fn serialize(&self) -> Vec<u8> {
        Vec::new()
    }

    pub fn deserialize<R: Read>(_reader: &mut R) -> std::io::Result<Self> {
        Ok(Self)
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgUnregisterGlobalCaptureMouse;

impl IcmMsgUnregisterGlobalCaptureMouse {
    pub fn serialize(&self) -> Vec<u8> {
        Vec::new()
    }

    pub fn deserialize<R: Read>(_reader: &mut R) -> std::io::Result<Self> {
        Ok(Self)
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowPosition {
    pub window_id: u32,
    pub x: i32,
    pub y: i32,
}

impl IcmMsgSetWindowPosition {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(12);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_i32::<LittleEndian>(self.x).unwrap();
        buf.write_i32::<LittleEndian>(self.y).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            x: reader.read_i32::<LittleEndian>()?,
            y: reader.read_i32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowSize {
    pub window_id: u32,
    pub width: u32,
    pub height: u32,
}

impl IcmMsgSetWindowSize {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(12);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            width: reader.read_u32::<LittleEndian>()?,
            height: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowOpacity {
    pub window_id: u32,
    pub opacity: f32,
}

impl IcmMsgSetWindowOpacity {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(8);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_f32::<LittleEndian>(self.opacity).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            opacity: reader.read_f32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowBlur {
    pub window_id: u32,
    pub blur_radius: f32,
    pub enabled: bool,
}

impl IcmMsgSetWindowBlur {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(9);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_f32::<LittleEndian>(self.blur_radius).unwrap();
        buf.write_u8(self.enabled as u8).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            blur_radius: reader.read_f32::<LittleEndian>()?,
            enabled: reader.read_u8()? != 0,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetScreenEffect {
    pub equation: String,
    pub enabled: bool,
}

impl IcmMsgSetScreenEffect {
    pub fn serialize(&self) -> Vec<u8> {
        let equation_bytes = self.equation.as_bytes();
        let mut buf = Vec::with_capacity(257);
        buf.extend_from_slice(&equation_bytes[..equation_bytes.len().min(256)]);
        for _ in equation_bytes.len()..256 {
            buf.push(0);
        }
        buf.write_u8(self.enabled as u8).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        let mut equation_bytes = [0u8; 256];
        reader.read_exact(&mut equation_bytes)?;
        let equation = String::from_utf8_lossy(&equation_bytes).trim_end_matches('\0').to_string();
        let enabled = reader.read_u8()? != 0;
        Ok(Self {
            equation,
            enabled,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowEffect {
    pub window_id: u32,
    pub equation: String,
    pub enabled: bool,
}

impl IcmMsgSetWindowEffect {
    pub fn serialize(&self) -> Vec<u8> {
        let equation_bytes = self.equation.as_bytes();
        let mut buf = Vec::with_capacity(261);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.extend_from_slice(&equation_bytes[..equation_bytes.len().min(256)]);
        for _ in equation_bytes.len()..256 {
            buf.push(0);
        }
        buf.write_u8(self.enabled as u8).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        let window_id = reader.read_u32::<LittleEndian>()?;
        let mut equation_bytes = [0u8; 256];
        reader.read_exact(&mut equation_bytes)?;
        let equation = String::from_utf8_lossy(&equation_bytes).trim_end_matches('\0').to_string();
        let enabled = reader.read_u8()? != 0;
        Ok(Self {
            window_id,
            equation,
            enabled,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowTransform {
    pub window_id: u32,
    pub scale_x: f32,
    pub scale_y: f32,
    pub rotation: f32,
}

impl IcmMsgSetWindowTransform {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(16);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_f32::<LittleEndian>(self.scale_x).unwrap();
        buf.write_f32::<LittleEndian>(self.scale_y).unwrap();
        buf.write_f32::<LittleEndian>(self.rotation).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            scale_x: reader.read_f32::<LittleEndian>()?,
            scale_y: reader.read_f32::<LittleEndian>()?,
            rotation: reader.read_f32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowLayer {
    pub window_id: u32,
    pub layer: i32,
}

impl IcmMsgSetWindowLayer {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(8);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_i32::<LittleEndian>(self.layer).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            layer: reader.read_i32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgRaiseWindow {
    pub window_id: u32,
}

impl IcmMsgRaiseWindow {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgLowerWindow {
    pub window_id: u32,
}

impl IcmMsgLowerWindow {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowParent {
    pub window_id: u32,
    pub parent_id: u32,
}

impl IcmMsgSetWindowParent {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(8);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.parent_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            parent_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowTransform3d {
    pub window_id: u32,
    pub translate_x: f32,
    pub translate_y: f32,
    pub translate_z: f32,
    pub rotate_x: f32,
    pub rotate_y: f32,
    pub rotate_z: f32,
    pub scale_x: f32,
    pub scale_y: f32,
    pub scale_z: f32,
}

impl IcmMsgSetWindowTransform3d {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(40);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_f32::<LittleEndian>(self.translate_x).unwrap();
        buf.write_f32::<LittleEndian>(self.translate_y).unwrap();
        buf.write_f32::<LittleEndian>(self.translate_z).unwrap();
        buf.write_f32::<LittleEndian>(self.rotate_x).unwrap();
        buf.write_f32::<LittleEndian>(self.rotate_y).unwrap();
        buf.write_f32::<LittleEndian>(self.rotate_z).unwrap();
        buf.write_f32::<LittleEndian>(self.scale_x).unwrap();
        buf.write_f32::<LittleEndian>(self.scale_y).unwrap();
        buf.write_f32::<LittleEndian>(self.scale_z).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            translate_x: reader.read_f32::<LittleEndian>()?,
            translate_y: reader.read_f32::<LittleEndian>()?,
            translate_z: reader.read_f32::<LittleEndian>()?,
            rotate_x: reader.read_f32::<LittleEndian>()?,
            rotate_y: reader.read_f32::<LittleEndian>()?,
            rotate_z: reader.read_f32::<LittleEndian>()?,
            scale_x: reader.read_f32::<LittleEndian>()?,
            scale_y: reader.read_f32::<LittleEndian>()?,
            scale_z: reader.read_f32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowMatrix {
    pub window_id: u32,
    pub matrix: [f32; 16],
}

impl IcmMsgSetWindowMatrix {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(68);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        for &val in &self.matrix {
            buf.write_f32::<LittleEndian>(val).unwrap();
        }
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        let window_id = reader.read_u32::<LittleEndian>()?;
        let mut matrix = [0.0f32; 16];
        for val in &mut matrix {
            *val = reader.read_f32::<LittleEndian>()?;
        }
        Ok(Self {
            window_id,
            matrix,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgAnimateWindow {
    pub window_id: u32,
    pub duration_ms: u32,
    pub target_x: f32,
    pub target_y: f32,
    pub target_scale_x: f32,
    pub target_scale_y: f32,
    pub target_opacity: f32,
    pub target_translate_x: f32,
    pub target_translate_y: f32,
    pub target_translate_z: f32,
    pub target_rotate_x: f32,
    pub target_rotate_y: f32,
    pub target_rotate_z: f32,
    pub target_scale_z: f32,
    pub flags: u32,
}

impl IcmMsgAnimateWindow {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(68);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.duration_ms).unwrap();
        buf.write_f32::<LittleEndian>(self.target_x).unwrap();
        buf.write_f32::<LittleEndian>(self.target_y).unwrap();
        buf.write_f32::<LittleEndian>(self.target_scale_x).unwrap();
        buf.write_f32::<LittleEndian>(self.target_scale_y).unwrap();
        buf.write_f32::<LittleEndian>(self.target_opacity).unwrap();
        buf.write_f32::<LittleEndian>(self.target_translate_x).unwrap();
        buf.write_f32::<LittleEndian>(self.target_translate_y).unwrap();
        buf.write_f32::<LittleEndian>(self.target_translate_z).unwrap();
        buf.write_f32::<LittleEndian>(self.target_rotate_x).unwrap();
        buf.write_f32::<LittleEndian>(self.target_rotate_y).unwrap();
        buf.write_f32::<LittleEndian>(self.target_rotate_z).unwrap();
        buf.write_f32::<LittleEndian>(self.target_scale_z).unwrap();
        buf.write_u32::<LittleEndian>(self.flags).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            duration_ms: reader.read_u32::<LittleEndian>()?,
            target_x: reader.read_f32::<LittleEndian>()?,
            target_y: reader.read_f32::<LittleEndian>()?,
            target_scale_x: reader.read_f32::<LittleEndian>()?,
            target_scale_y: reader.read_f32::<LittleEndian>()?,
            target_opacity: reader.read_f32::<LittleEndian>()?,
            target_translate_x: reader.read_f32::<LittleEndian>()?,
            target_translate_y: reader.read_f32::<LittleEndian>()?,
            target_translate_z: reader.read_f32::<LittleEndian>()?,
            target_rotate_x: reader.read_f32::<LittleEndian>()?,
            target_rotate_y: reader.read_f32::<LittleEndian>()?,
            target_rotate_z: reader.read_f32::<LittleEndian>()?,
            target_scale_z: reader.read_f32::<LittleEndian>()?,
            flags: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgStopAnimation {
    pub window_id: u32,
}

impl IcmMsgStopAnimation {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgSetWindowState {
    pub window_id: u32,
    pub state: u32,
}

impl IcmMsgSetWindowState {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(8);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.state).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            state: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgFocusWindow {
    pub window_id: u32,
}

impl IcmMsgFocusWindow {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgBlurWindow {
    pub window_id: u32,
}

impl IcmMsgBlurWindow {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryWindowPosition {
    pub window_id: u32,
}

impl IcmMsgQueryWindowPosition {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryWindowSize {
    pub window_id: u32,
}

impl IcmMsgQueryWindowSize {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryWindowAttributes {
    pub window_id: u32,
}

impl IcmMsgQueryWindowAttributes {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryWindowLayer {
    pub window_id: u32,
}

impl IcmMsgQueryWindowLayer {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryWindowState {
    pub window_id: u32,
}

impl IcmMsgQueryWindowState {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgWindowPositionData {
    pub window_id: u32,
    pub x: i32,
    pub y: i32,
}

impl IcmMsgWindowPositionData {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(12);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_i32::<LittleEndian>(self.x).unwrap();
        buf.write_i32::<LittleEndian>(self.y).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            x: reader.read_i32::<LittleEndian>()?,
            y: reader.read_i32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgWindowSizeData {
    pub window_id: u32,
    pub width: u32,
    pub height: u32,
}

impl IcmMsgWindowSizeData {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(12);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            width: reader.read_u32::<LittleEndian>()?,
            height: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgWindowAttributesData {
    pub window_id: u32,
    pub visible: bool,
    pub opacity: f32,
    pub scale_x: f32,
    pub scale_y: f32,
    pub rotation: f32,
}

impl IcmMsgWindowAttributesData {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(21);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u8(self.visible as u8).unwrap();
        buf.write_f32::<LittleEndian>(self.opacity).unwrap();
        buf.write_f32::<LittleEndian>(self.scale_x).unwrap();
        buf.write_f32::<LittleEndian>(self.scale_y).unwrap();
        buf.write_f32::<LittleEndian>(self.rotation).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            visible: reader.read_u8()? != 0,
            opacity: reader.read_f32::<LittleEndian>()?,
            scale_x: reader.read_f32::<LittleEndian>()?,
            scale_y: reader.read_f32::<LittleEndian>()?,
            rotation: reader.read_f32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgWindowLayerData {
    pub window_id: u32,
    pub layer: i32,
    pub parent_id: u32,
}

impl IcmMsgWindowLayerData {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(12);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_i32::<LittleEndian>(self.layer).unwrap();
        buf.write_u32::<LittleEndian>(self.parent_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            layer: reader.read_i32::<LittleEndian>()?,
            parent_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgWindowStateData {
    pub window_id: u32,
    pub state: u32,
    pub focused: bool,
}

impl IcmMsgWindowStateData {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(9);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_u32::<LittleEndian>(self.state).unwrap();
        buf.write_u8(self.focused as u8).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
            state: reader.read_u32::<LittleEndian>()?,
            focused: reader.read_u8()? != 0,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryScreenDimensions;

impl IcmMsgQueryScreenDimensions {
    pub fn serialize(&self) -> Vec<u8> {
        Vec::new()
    }

    pub fn deserialize<R: Read>(_reader: &mut R) -> std::io::Result<Self> {
        Ok(Self)
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgScreenDimensionsData {
    pub total_width: u32,
    pub total_height: u32,
    pub scale: f32,
}

impl IcmMsgScreenDimensionsData {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(12);
        buf.write_u32::<LittleEndian>(self.total_width).unwrap();
        buf.write_u32::<LittleEndian>(self.total_height).unwrap();
        buf.write_f32::<LittleEndian>(self.scale).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            total_width: reader.read_u32::<LittleEndian>()?,
            total_height: reader.read_u32::<LittleEndian>()?,
            scale: reader.read_f32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMonitorInfo {
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub physical_width: u32,
    pub physical_height: u32,
    pub refresh_rate: u32,
    pub scale: f32,
    pub enabled: bool,
    pub primary: bool,
    pub name: String,
}

impl IcmMonitorInfo {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(48);
        buf.write_i32::<LittleEndian>(self.x).unwrap();
        buf.write_i32::<LittleEndian>(self.y).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf.write_u32::<LittleEndian>(self.physical_width).unwrap();
        buf.write_u32::<LittleEndian>(self.physical_height).unwrap();
        buf.write_u32::<LittleEndian>(self.refresh_rate).unwrap();
        buf.write_f32::<LittleEndian>(self.scale).unwrap();
        buf.write_u8(self.enabled as u8).unwrap();
        buf.write_u8(self.primary as u8).unwrap();
        let name_bytes = self.name.as_bytes();
        buf.extend_from_slice(&name_bytes[..name_bytes.len().min(32)]);
        for _ in name_bytes.len()..32 {
            buf.push(0);
        }
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        let x = reader.read_i32::<LittleEndian>()?;
        let y = reader.read_i32::<LittleEndian>()?;
        let width = reader.read_u32::<LittleEndian>()?;
        let height = reader.read_u32::<LittleEndian>()?;
        let physical_width = reader.read_u32::<LittleEndian>()?;
        let physical_height = reader.read_u32::<LittleEndian>()?;
        let refresh_rate = reader.read_u32::<LittleEndian>()?;
        let scale = reader.read_f32::<LittleEndian>()?;
        let enabled = reader.read_u8()? != 0;
        let primary = reader.read_u8()? != 0;
        let mut name_bytes = [0u8; 32];
        reader.read_exact(&mut name_bytes)?;
        let name = String::from_utf8_lossy(&name_bytes).trim_end_matches('\0').to_string();
        Ok(Self {
            x,
            y,
            width,
            height,
            physical_width,
            physical_height,
            refresh_rate,
            scale,
            enabled,
            primary,
            name,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryMonitors;

impl IcmMsgQueryMonitors {
    pub fn serialize(&self) -> Vec<u8> {
        Vec::new()
    }

    pub fn deserialize<R: Read>(_reader: &mut R) -> std::io::Result<Self> {
        Ok(Self)
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgMonitorsData {
    pub monitors: Vec<IcmMonitorInfo>,
}

impl IcmMsgMonitorsData {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4 + self.monitors.len() * 48);
        buf.write_u32::<LittleEndian>(self.monitors.len() as u32).unwrap();
        for monitor in &self.monitors {
            buf.extend_from_slice(&monitor.serialize());
        }
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        let num_monitors = reader.read_u32::<LittleEndian>()? as usize;
        let mut monitors = Vec::with_capacity(num_monitors);
        for _ in 0..num_monitors {
            monitors.push(IcmMonitorInfo::deserialize(reader)?);
        }
        Ok(Self {
            monitors,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgCompositorShutdown;

impl IcmMsgCompositorShutdown {
    pub fn serialize(&self) -> Vec<u8> {
        Vec::new()
    }

    pub fn deserialize<R: Read>(_reader: &mut R) -> std::io::Result<Self> {
        Ok(Self)
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryWindowInfo {
    pub window_id: u32,
}

impl IcmMsgQueryWindowInfo {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        Ok(Self {
            window_id: reader.read_u32::<LittleEndian>()?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgWindowInfoData {
    pub window_id: u32,
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub visible: bool,
    pub opacity: f32,
    pub scale_x: f32,
    pub scale_y: f32,
    pub rotation: f32,
    pub layer: i32,
    pub parent_id: u32,
    pub state: u32,
    pub focused: bool,
    pub pid: u32,
    pub process_name: String,
}

impl IcmMsgWindowInfoData {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(59 + self.process_name.len() + 1);
        buf.write_u32::<LittleEndian>(self.window_id).unwrap();
        buf.write_i32::<LittleEndian>(self.x).unwrap();
        buf.write_i32::<LittleEndian>(self.y).unwrap();
        buf.write_u32::<LittleEndian>(self.width).unwrap();
        buf.write_u32::<LittleEndian>(self.height).unwrap();
        buf.write_u8(self.visible as u8).unwrap();
        buf.write_f32::<LittleEndian>(self.opacity).unwrap();
        buf.write_f32::<LittleEndian>(self.scale_x).unwrap();
        buf.write_f32::<LittleEndian>(self.scale_y).unwrap();
        buf.write_f32::<LittleEndian>(self.rotation).unwrap();
        buf.write_i32::<LittleEndian>(self.layer).unwrap();
        buf.write_u32::<LittleEndian>(self.parent_id).unwrap();
        buf.write_u32::<LittleEndian>(self.state).unwrap();
        buf.write_u8(self.focused as u8).unwrap();
        buf.write_u32::<LittleEndian>(self.pid).unwrap();
        let process_name_bytes = self.process_name.as_bytes();
        buf.extend_from_slice(process_name_bytes);
        buf.push(0); // null terminator
        buf
    }

    pub fn deserialize<R: Read>(reader: &mut R) -> std::io::Result<Self> {
        let window_id = reader.read_u32::<LittleEndian>()?;
        let x = reader.read_i32::<LittleEndian>()?;
        let y = reader.read_i32::<LittleEndian>()?;
        let width = reader.read_u32::<LittleEndian>()?;
        let height = reader.read_u32::<LittleEndian>()?;
        let visible = reader.read_u8()? != 0;
        let opacity = reader.read_f32::<LittleEndian>()?;
        let scale_x = reader.read_f32::<LittleEndian>()?;
        let scale_y = reader.read_f32::<LittleEndian>()?;
        let rotation = reader.read_f32::<LittleEndian>()?;
        let layer = reader.read_i32::<LittleEndian>()?;
        let parent_id = reader.read_u32::<LittleEndian>()?;
        let state = reader.read_u32::<LittleEndian>()?;
        let focused = reader.read_u8()? != 0;
        let pid = reader.read_u32::<LittleEndian>()?;
        let mut process_name_bytes = Vec::new();
        let mut byte = [0u8];
        loop {
            reader.read_exact(&mut byte)?;
            if byte[0] == 0 {
                break;
            }
            process_name_bytes.push(byte[0]);
        }
        let process_name = String::from_utf8(process_name_bytes).map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e))?;
        Ok(Self {
            window_id,
            x,
            y,
            width,
            height,
            visible,
            opacity,
            scale_x,
            scale_y,
            rotation,
            layer,
            parent_id,
            state,
            focused,
            pid,
            process_name,
        })
    }
}

pub const ICM_WINDOW_ID_ROOT: u32 = 0;

#[derive(Debug, Clone)]
pub struct IcmMeshVertex {
    pub x: f32,
    pub y: f32,
    pub u: f32,
    pub v: f32,
}

#[derive(Debug, Clone)]
pub struct IcmMsgQueryToplevelWindows {
    pub flags: u32,  // 0 = all windows, 1 = visible only
}

impl IcmMsgQueryToplevelWindows {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.flags).unwrap();
        buf
    }
}

#[derive(Debug, Clone)]
pub struct IcmToplevelWindowEntry {
    pub window_id: u32,
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub visible: bool,
    pub focused: bool,
    pub state: u32,
    pub title: String,
    pub app_id: String,
}

#[derive(Debug, Clone)]
pub struct IcmMsgSubscribeWindowEvents {
    pub event_mask: u32,
}

impl IcmMsgSubscribeWindowEvents {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4);
        buf.write_u32::<LittleEndian>(self.event_mask).unwrap();
        buf
    }
}

#[derive(Debug, Clone)]
pub struct IcmMsgLaunchApp {
    pub command: String,
}

impl IcmMsgLaunchApp {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(4 + self.command.len() + 1);
        buf.write_u32::<LittleEndian>((self.command.len() + 1) as u32).unwrap();
        buf.extend_from_slice(self.command.as_bytes());
        buf.push(0); // null terminator
        buf
    }
}

// Client for communicating with the ICM compositor
pub struct IcmClient {
    socket: UnixStream,
}

impl IcmClient {
    pub fn connect(socket_path: &str) -> std::io::Result<Self> {
        let socket = UnixStream::connect(socket_path)?;
        socket.set_nonblocking(true)?;
        Ok(Self { socket })
    }

    pub fn send_message(&mut self, msg_type: IcmIpcMsgType, payload: &[u8]) -> std::io::Result<()> {
        let header = IcmIpcHeader::new(msg_type, payload.len());
        let header_buf = header.serialize();
        self.socket.write_all(&header_buf)?;
        if !payload.is_empty() {
            self.socket.write_all(payload)?;
        }
        Ok(())
    }

    pub fn receive_message(&mut self) -> std::io::Result<(IcmIpcHeader, Vec<u8>)> {
        let header = IcmIpcHeader::deserialize(&mut self.socket)?;
        let mut payload = vec![0u8; (header.length - 16) as usize];
        if !payload.is_empty() {
            self.socket.read_exact(&mut payload)?;
        }
        Ok((header, payload))
    }
}