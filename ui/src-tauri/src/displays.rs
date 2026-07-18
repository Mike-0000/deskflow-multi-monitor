use crate::config::DisplayRect;

const REFERENCE_DPI: i32 = 96;

pub fn layout_size_from_pixels(pixels: i32, dpi: i32, scale: f32) -> i32 {
    if pixels <= 0 {
        return 0;
    }
    if scale > 0.0 && (scale - 1.0).abs() > 0.001 {
        return ((pixels as f64 / scale as f64).round() as i32).max(1);
    }
    let safe_dpi = if dpi > 0 { dpi } else { REFERENCE_DPI };
    ((((pixels as i64) * (REFERENCE_DPI as i64) + (safe_dpi as i64) / 2) / (safe_dpi as i64)) as i32)
        .max(1)
}

pub fn ensure_layout_sizes(monitor: &mut DisplayRect) {
    if monitor.layout_width <= 0 {
        monitor.layout_width =
            layout_size_from_pixels(monitor.width, monitor.dpi, monitor.scale);
    }
    if monitor.layout_height <= 0 {
        monitor.layout_height =
            layout_size_from_pixels(monitor.height, monitor.dpi, monitor.scale);
    }
}

/// Enumerate local OS displays for the server machine (mirrors Qt QGuiApplication::screens).
pub fn enumerate_local_displays() -> Result<Vec<DisplayRect>, String> {
    #[cfg(windows)]
    {
        windows_enumerate()
    }
    #[cfg(not(windows))]
    {
        Err("local display import is only implemented on Windows in this build".into())
    }
}

#[cfg(windows)]
fn windows_enumerate() -> Result<Vec<DisplayRect>, String> {
    use std::mem::{size_of, zeroed};
    use windows_sys::Win32::Foundation::{BOOL, LPARAM, RECT};
    use windows_sys::Win32::Graphics::Gdi::{
        EnumDisplayMonitors, GetMonitorInfoW, HDC, HMONITOR, MONITORINFOEXW,
    };

    struct Ctx {
        displays: Vec<DisplayRect>,
    }

    unsafe extern "system" fn callback(
        monitor: HMONITOR,
        _hdc: HDC,
        _rect: *mut RECT,
        data: LPARAM,
    ) -> BOOL {
        let ctx = &mut *(data as *mut Ctx);
        let mut info: MONITORINFOEXW = zeroed();
        info.monitorInfo.cbSize = size_of::<MONITORINFOEXW>() as u32;
        if GetMonitorInfoW(monitor, &mut info as *mut _ as *mut _) == 0 {
            return 1;
        }

        let rc = info.monitorInfo.rcMonitor;
        let width = rc.right - rc.left;
        let height = rc.bottom - rc.top;
        let name = {
            let len = info
                .szDevice
                .iter()
                .position(|&c| c == 0)
                .unwrap_or(info.szDevice.len());
            String::from_utf16_lossy(&info.szDevice[..len])
        };

        // Logical DPI approximation: Windows 96 * scale. Prefer 96 for layout seed.
        let dpi = REFERENCE_DPI;
        let scale = 1.0f32;
        let mut monitor = DisplayRect {
            id: name.clone(),
            name,
            world_x: layout_size_from_pixels(rc.left, dpi, scale),
            world_y: layout_size_from_pixels(rc.top, dpi, scale),
            width,
            height,
            local_x: rc.left,
            local_y: rc.top,
            scale,
            dpi,
            layout_width: 0,
            layout_height: 0,
            needs_placement: false,
        };
        ensure_layout_sizes(&mut monitor);
        ctx.displays.push(monitor);
        1
    }

    let mut ctx = Ctx {
        displays: Vec::new(),
    };
    let ok = unsafe {
        EnumDisplayMonitors(
            std::ptr::null_mut(),
            std::ptr::null(),
            Some(callback),
            &mut ctx as *mut _ as LPARAM,
        )
    };
    if ok == 0 {
        return Err("EnumDisplayMonitors failed".into());
    }
    if ctx.displays.is_empty() {
        return Err("no displays found".into());
    }
    Ok(ctx.displays)
}
