use serde::{Deserialize, Serialize};
use std::fs;
use std::path::Path;

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
pub struct ScreenCell {
    pub name: String,
    pub aliases: Vec<String>,
    pub is_server: bool,
    pub column: usize,
    pub row: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
pub struct DisplayRect {
    pub id: String,
    pub name: String,
    pub world_x: i32,
    pub world_y: i32,
    pub width: i32,
    pub height: i32,
    pub local_x: i32,
    pub local_y: i32,
    pub scale: f32,
    pub dpi: i32,
    #[serde(default)]
    pub layout_width: i32,
    #[serde(default)]
    pub layout_height: i32,
    #[serde(default)]
    pub needs_placement: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
pub struct MachineLayout {
    pub name: String,
    pub monitors: Vec<DisplayRect>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
pub struct WorkspaceLayout {
    pub enabled: bool,
    #[serde(default = "default_layout_version")]
    pub version: i32,
    pub machines: Vec<MachineLayout>,
}

fn default_layout_version() -> i32 {
    2
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ServerConfig {
    pub columns: usize,
    pub rows: usize,
    pub screens: Vec<ScreenCell>,
    pub protocol: String,
    pub has_heartbeat: bool,
    pub heartbeat: i32,
    pub relative_mouse_moves: bool,
    pub win32_keep_foreground: bool,
    pub has_switch_delay: bool,
    pub switch_delay: i32,
    pub has_switch_double_tap: bool,
    pub switch_double_tap: i32,
    pub switch_corner_size: i32,
    pub clipboard_sharing: bool,
    pub clipboard_sharing_size: i32,
    pub default_lock_to_screen: bool,
    pub disable_lock_to_screen: bool,
    pub workspace_layout: WorkspaceLayout,
}

impl Default for ServerConfig {
    fn default() -> Self {
        let columns = 5;
        let rows = 3;
        let mut screens = Vec::with_capacity(columns * rows);
        for row in 0..rows {
            for column in 0..columns {
                screens.push(ScreenCell {
                    name: String::new(),
                    aliases: vec![],
                    is_server: false,
                    column,
                    row,
                });
            }
        }
        // Default server cell at index 7 (middle of 5x3) matching Qt
        if let Some(cell) = screens.get_mut(7) {
            let hostname = hostname::get()
                .ok()
                .and_then(|h| h.into_string().ok())
                .unwrap_or_else(|| "server".into());
            cell.name = hostname;
            cell.is_server = true;
        }
        Self {
            columns,
            rows,
            screens,
            protocol: "barrier".into(),
            has_heartbeat: false,
            heartbeat: 5000,
            relative_mouse_moves: false,
            win32_keep_foreground: false,
            has_switch_delay: false,
            switch_delay: 250,
            has_switch_double_tap: false,
            switch_double_tap: 250,
            switch_corner_size: 0,
            clipboard_sharing: true,
            clipboard_sharing_size: 10 * 1024 * 1024,
            default_lock_to_screen: false,
            disable_lock_to_screen: false,
            workspace_layout: WorkspaceLayout::default(),
        }
    }
}

impl ServerConfig {
    pub fn index(&self, column: usize, row: usize) -> Option<usize> {
        if column >= self.columns || row >= self.rows {
            return None;
        }
        Some(row * self.columns + column)
    }

    pub fn adjacent(&self, index: usize, dx: isize, dy: isize) -> Option<usize> {
        let col = (index % self.columns) as isize + dx;
        let row = (index / self.columns) as isize + dy;
        if col < 0 || row < 0 {
            return None;
        }
        self.index(col as usize, row as usize)
    }

    pub fn ensure_minimal_two_screen_layout(&mut self, client_name: &str) {
        let server_name = self
            .screens
            .iter()
            .find(|s| s.is_server)
            .map(|s| s.name.clone())
            .filter(|n| !n.is_empty())
            .unwrap_or_else(|| {
                hostname::get()
                    .ok()
                    .and_then(|h| h.into_string().ok())
                    .unwrap_or_else(|| "server".into())
            });

        for s in &mut self.screens {
            s.name.clear();
            s.is_server = false;
            s.aliases.clear();
        }

        if let Some(server) = self.screens.get_mut(7) {
            server.name = server_name;
            server.is_server = true;
        }
        if let Some(client) = self.screens.get_mut(8) {
            client.name = if client_name.is_empty() {
                "client".into()
            } else {
                client_name.into()
            };
        }
    }

    pub fn add_screen_name(&mut self, name: &str) -> bool {
        if name.is_empty() {
            return false;
        }
        if self.screens.iter().any(|s| s.name == name) {
            return false;
        }
        if let Some(cell) = self.screens.iter_mut().find(|s| s.name.is_empty()) {
            cell.name = name.to_string();
            return true;
        }
        false
    }

    pub fn workspace_layout_mut(&mut self) -> &mut WorkspaceLayout {
        &mut self.workspace_layout
    }

    /// Ensure every named screen has a MachineLayout entry (Qt rebuildMachineList).
    pub fn sync_machines_from_screens(&mut self) {
        let names: Vec<String> = self
            .screens
            .iter()
            .filter(|s| !s.name.is_empty())
            .map(|s| s.name.clone())
            .collect();
        for name in &names {
            if !self
                .workspace_layout
                .machines
                .iter()
                .any(|m| m.name == *name)
            {
                self.workspace_layout.machines.push(MachineLayout {
                    name: name.clone(),
                    monitors: vec![],
                });
            }
        }
        // Drop machines that no longer exist in the screen list
        self.workspace_layout
            .machines
            .retain(|m| names.iter().any(|n| n == &m.name));
    }

    pub fn save_conf(&self, path: &Path) -> Result<(), String> {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }
        fs::write(path, self.to_conf_string()).map_err(|e| e.to_string())
    }

    pub fn load_conf(path: &Path) -> Result<Self, String> {
        let text = fs::read_to_string(path).map_err(|e| e.to_string())?;
        Ok(Self::from_conf_string(&text))
    }

    pub fn to_conf_string(&self) -> String {
        let mut out = String::new();
        out.push_str("section: screens\n");
        for screen in self.screens.iter().filter(|s| !s.name.is_empty()) {
            out.push_str(&format!("\t{}:\n", screen.name));
            out.push_str("\t\thalfDuplexCapsLock = false\n");
            out.push_str("\t\thalfDuplexNumLock = false\n");
            out.push_str("\t\thalfDuplexScrollLock = false\n");
            // Do not emit stickyKeys — deskflow-core Config rejects unknown screen keys.
            out.push_str("\t\tswitchCorners = none\n");
            out.push_str("\t\tswitchCornerSize = 0\n");
        }
        out.push_str("end\n\n");

        out.push_str("section: aliases\n");
        for screen in self.screens.iter().filter(|s| !s.name.is_empty() && !s.aliases.is_empty()) {
            out.push_str(&format!("\t{}:\n", screen.name));
            for alias in &screen.aliases {
                out.push_str(&format!("\t\t{alias}\n"));
            }
        }
        out.push_str("end\n\n");

        out.push_str("section: links\n");
        for (i, screen) in self.screens.iter().enumerate() {
            if screen.name.is_empty() {
                continue;
            }
            out.push_str(&format!("\t{}:\n", screen.name));
            for (dx, dy, name) in [(1, 0, "right"), (-1, 0, "left"), (0, -1, "up"), (0, 1, "down")] {
                if let Some(idx) = self.adjacent(i, dx, dy) {
                    let neighbor = &self.screens[idx];
                    if !neighbor.name.is_empty() {
                        out.push_str(&format!("\t\t{name} = {}\n", neighbor.name));
                    }
                }
            }
        }
        out.push_str("end\n\n");

        if self.workspace_layout.enabled || !self.workspace_layout.machines.is_empty() {
            out.push_str("section: display_layouts\n");
            out.push_str(&format!("\tversion = {}\n", self.workspace_layout.version.max(2)));
            out.push_str(&format!(
                "\tadvancedLayout = {}\n",
                if self.workspace_layout.enabled {
                    "true"
                } else {
                    "false"
                }
            ));
            for machine in &self.workspace_layout.machines {
                out.push_str(&format!("\t{}:\n", machine.name));
                for monitor in &machine.monitors {
                    let id = if monitor.id.is_empty() {
                        &monitor.name
                    } else {
                        &monitor.id
                    };
                    out.push_str(&format!("\t\t{id}:\n"));
                    if !monitor.id.is_empty() {
                        out.push_str(&format!("\t\t\tid = {}\n", monitor.id));
                    }
                    if !monitor.name.is_empty() {
                        out.push_str(&format!("\t\t\tname = {}\n", monitor.name));
                    }
                    out.push_str(&format!("\t\t\tworldX = {}\n", monitor.world_x));
                    out.push_str(&format!("\t\t\tworldY = {}\n", monitor.world_y));
                    out.push_str(&format!("\t\t\twidth = {}\n", monitor.width));
                    out.push_str(&format!("\t\t\theight = {}\n", monitor.height));
                    out.push_str(&format!("\t\t\tlocalX = {}\n", monitor.local_x));
                    out.push_str(&format!("\t\t\tlocalY = {}\n", monitor.local_y));
                    let lw = if monitor.layout_width > 0 {
                        monitor.layout_width
                    } else {
                        monitor.width
                    };
                    let lh = if monitor.layout_height > 0 {
                        monitor.layout_height
                    } else {
                        monitor.height
                    };
                    out.push_str(&format!("\t\t\tlayoutWidth = {lw}\n"));
                    out.push_str(&format!("\t\t\tlayoutHeight = {lh}\n"));
                    if (monitor.scale - 1.0).abs() > f32::EPSILON {
                        out.push_str(&format!("\t\t\tscale = {}\n", monitor.scale));
                    }
                    if monitor.dpi != 96 && monitor.dpi != 0 {
                        out.push_str(&format!("\t\t\tdpi = {}\n", monitor.dpi));
                    }
                    if monitor.needs_placement {
                        out.push_str("\t\t\tneedsPlacement = true\n");
                    }
                }
            }
            out.push_str("end\n\n");
        }

        out.push_str("section: options\n");
        if self.has_heartbeat {
            out.push_str(&format!("\theartbeat = {}\n", self.heartbeat));
        }
        out.push_str(&format!("\tprotocol = {}\n", self.protocol.to_lowercase()));
        out.push_str(&format!(
            "\trelativeMouseMoves = {}\n",
            bool_word(self.relative_mouse_moves)
        ));
        out.push_str(&format!(
            "\twin32KeepForeground = {}\n",
            bool_word(self.win32_keep_foreground)
        ));
        out.push_str(&format!(
            "\tdefaultLockToScreenState = {}\n",
            bool_word(self.default_lock_to_screen)
        ));
        out.push_str(&format!(
            "\tdisableLockToScreen = {}\n",
            bool_word(self.disable_lock_to_screen)
        ));
        out.push_str(&format!(
            "\tclipboardSharing = {}\n",
            bool_word(self.clipboard_sharing)
        ));
        out.push_str(&format!(
            "\tclipboardSharingSize = {}\n",
            self.clipboard_sharing_size
        ));
        if self.has_switch_delay {
            out.push_str(&format!("\tswitchDelay = {}\n", self.switch_delay));
        }
        if self.has_switch_double_tap {
            out.push_str(&format!("\tswitchDoubleTap = {}\n", self.switch_double_tap));
        }
        out.push_str("\tswitchCorners = none \n");
        out.push_str(&format!("\tswitchCornerSize = {}\n", self.switch_corner_size));
        out.push_str("end\n\n");
        out
    }

    pub fn from_conf_string(text: &str) -> Self {
        let mut config = ServerConfig::default();
        // Clear default screens for parse
        for s in &mut config.screens {
            s.name.clear();
            s.is_server = false;
        }

        let mut section = String::new();
        let mut current_screen: Option<String> = None;
        let mut screen_names: Vec<String> = Vec::new();
        let mut links: Vec<(String, String, String)> = Vec::new();

        for raw in text.lines() {
            let line = raw.trim();
            if line.is_empty() || line.starts_with('#') {
                continue;
            }
            if let Some(rest) = line.strip_prefix("section:") {
                section = rest.trim().to_string();
                current_screen = None;
                continue;
            }
            if line == "end" {
                section.clear();
                current_screen = None;
                continue;
            }

            match section.as_str() {
                "screens" => {
                    if let Some(name) = line.strip_suffix(':') {
                        let name = name.trim().to_string();
                        screen_names.push(name.clone());
                        current_screen = Some(name);
                    }
                }
                "links" => {
                    if let Some(name) = line.strip_suffix(':') {
                        current_screen = Some(name.trim().to_string());
                    } else if let Some((dir, target)) = line.split_once('=') {
                        if let Some(from) = &current_screen {
                            links.push((
                                from.clone(),
                                dir.trim().to_string(),
                                target.trim().to_string(),
                            ));
                        }
                    }
                }
                "options" => {
                    if let Some((k, v)) = line.split_once('=') {
                        let k = k.trim();
                        let v = v.trim();
                        match k {
                            "protocol" => config.protocol = v.to_string(),
                            "heartbeat" => {
                                config.has_heartbeat = true;
                                config.heartbeat = v.parse().unwrap_or(5000);
                            }
                            "relativeMouseMoves" => {
                                config.relative_mouse_moves = v == "true";
                            }
                            "win32KeepForeground" => {
                                config.win32_keep_foreground = v == "true";
                            }
                            "clipboardSharing" => config.clipboard_sharing = v == "true",
                            "clipboardSharingSize" => {
                                config.clipboard_sharing_size = v.parse().unwrap_or(config.clipboard_sharing_size);
                            }
                            "switchDelay" => {
                                config.has_switch_delay = true;
                                config.switch_delay = v.parse().unwrap_or(250);
                            }
                            "switchDoubleTap" => {
                                config.has_switch_double_tap = true;
                                config.switch_double_tap = v.parse().unwrap_or(250);
                            }
                            "switchCornerSize" => {
                                config.switch_corner_size = v.parse().unwrap_or(0);
                            }
                            "defaultLockToScreenState" => {
                                config.default_lock_to_screen = v == "true";
                            }
                            "disableLockToScreen" => {
                                config.disable_lock_to_screen = v == "true";
                            }
                            _ => {}
                        }
                    }
                }
                "display_layouts" => {
                    // Indentation-aware parse of machine / monitor blocks
                }
                _ => {}
            }
        }

        // Full display_layouts parse (separate pass for nesting)
        parse_display_layouts(text, &mut config);

        // Reconstruct the editor grid from the links section. Previously links were
        // parsed then discarded, so Save layout appeared to work until reload.
        place_screens_from_links(&screen_names, &links, &mut config);

        config
    }

    /// Mark the screen matching `server_name` as the server (case-insensitive).
    pub fn mark_server_by_name(&mut self, server_name: &str) {
        if server_name.is_empty() {
            return;
        }
        let mut matched = false;
        for screen in &mut self.screens {
            let is = !screen.name.is_empty()
                && screen.name.eq_ignore_ascii_case(server_name);
            screen.is_server = is;
            matched |= is;
        }
        if !matched {
            if let Some(first) = self.screens.iter_mut().find(|s| !s.name.is_empty()) {
                first.is_server = true;
            }
        }
    }
}

/// Place named screens onto the columns×rows grid using adjacency from `links`.
/// `links` entries are `(from, direction, to)` where direction is left/right/up/down.
fn place_screens_from_links(
    screen_names: &[String],
    links: &[(String, String, String)],
    config: &mut ServerConfig,
) {
    use std::collections::{HashMap, VecDeque};

    for screen in &mut config.screens {
        screen.name.clear();
        screen.is_server = false;
        screen.aliases.clear();
    }

    if screen_names.is_empty() {
        return;
    }

    let mut adj: HashMap<String, Vec<(String, String)>> = HashMap::new();
    for (from, dir, to) in links {
        adj.entry(from.clone())
            .or_default()
            .push((dir.to_lowercase(), to.clone()));
    }

    let delta = |dir: &str| -> Option<(i32, i32)> {
        match dir {
            "right" => Some((1, 0)),
            "left" => Some((-1, 0)),
            "up" => Some((0, -1)),
            "down" => Some((0, 1)),
            _ => None,
        }
    };

    let mut pos: HashMap<String, (i32, i32)> = HashMap::new();
    let mut queue = VecDeque::new();
    let start = screen_names
        .iter()
        .find(|n| adj.contains_key(n.as_str()))
        .cloned()
        .unwrap_or_else(|| screen_names[0].clone());
    pos.insert(start.clone(), (0, 0));
    queue.push_back(start);

    while let Some(name) = queue.pop_front() {
        let (x, y) = pos[&name];
        let neighbors = adj.get(&name).cloned().unwrap_or_default();
        for (dir, to) in neighbors {
            let Some((dx, dy)) = delta(dir.as_str()) else {
                continue;
            };
            if pos.contains_key(&to) {
                continue;
            }
            pos.insert(to.clone(), (x + dx, y + dy));
            queue.push_back(to);
        }
    }

    let mut next_x = pos.values().map(|(x, _)| *x).max().unwrap_or(0) + 1;
    for name in screen_names {
        if let std::collections::hash_map::Entry::Vacant(e) = pos.entry(name.clone()) {
            e.insert((next_x, 0));
            next_x += 1;
        }
    }

    let min_x = pos.values().map(|(x, _)| *x).min().unwrap_or(0);
    let min_y = pos.values().map(|(_, y)| *y).min().unwrap_or(0);
    let max_x = pos.values().map(|(x, _)| *x).max().unwrap_or(0);
    let max_y = pos.values().map(|(_, y)| *y).max().unwrap_or(0);
    let width = (max_x - min_x + 1).max(1);
    let height = (max_y - min_y + 1).max(1);
    let origin_col = ((config.columns as i32 - width) / 2).max(0);
    let origin_row = ((config.rows as i32 - height) / 2).max(0);

    let mut overflow_col = 0usize;
    for name in screen_names {
        let Some(&(x, y)) = pos.get(name) else {
            continue;
        };
        let col = (origin_col + (x - min_x)) as usize;
        let row = (origin_row + (y - min_y)) as usize;
        if let Some(idx) = config.index(col, row) {
            if config.screens[idx].name.is_empty() {
                config.screens[idx].name = name.clone();
                config.screens[idx].column = col;
                config.screens[idx].row = row;
                continue;
            }
        }
        // Collision / out-of-bounds fallback: pack remaining cells left-to-right.
        while overflow_col < config.screens.len() && !config.screens[overflow_col].name.is_empty()
        {
            overflow_col += 1;
        }
        if overflow_col < config.screens.len() {
            let c = overflow_col % config.columns;
            let r = overflow_col / config.columns;
            config.screens[overflow_col].name = name.clone();
            config.screens[overflow_col].column = c;
            config.screens[overflow_col].row = r;
            overflow_col += 1;
        }
    }

    // First screen in the conf is a reasonable default server until settings mark one.
    if let Some(first) = screen_names.first() {
        for screen in &mut config.screens {
            screen.is_server = screen.name == *first;
        }
    }
}

fn parse_display_layouts(text: &str, config: &mut ServerConfig) {
    let mut in_section = false;
    let mut current_machine: Option<String> = None;
    let mut current_monitor: Option<DisplayRect> = None;

    let flush_monitor = |config: &mut ServerConfig,
                         machine: &Option<String>,
                         monitor: &mut Option<DisplayRect>| {
        if let (Some(mname), Some(mon)) = (machine, monitor.take()) {
            if let Some(machine) = config
                .workspace_layout
                .machines
                .iter_mut()
                .find(|m| m.name == *mname)
            {
                machine.monitors.push(mon);
            } else {
                config.workspace_layout.machines.push(MachineLayout {
                    name: mname.clone(),
                    monitors: vec![mon],
                });
            }
        }
    };

    for raw in text.lines() {
        let trimmed = raw.trim();
        if trimmed.is_empty() || trimmed.starts_with('#') {
            continue;
        }
        if let Some(rest) = trimmed.strip_prefix("section:") {
            if in_section {
                flush_monitor(config, &current_machine, &mut current_monitor);
            }
            in_section = rest.trim() == "display_layouts";
            current_machine = None;
            current_monitor = None;
            continue;
        }
        if !in_section {
            continue;
        }
        if trimmed == "end" {
            flush_monitor(config, &current_machine, &mut current_monitor);
            in_section = false;
            current_machine = None;
            current_monitor = None;
            continue;
        }

        // Detect nesting by leading tabs/spaces on the raw line
        let indent = raw.chars().take_while(|c| *c == '\t' || *c == ' ').count();
        let spaces = if raw.starts_with('\t') {
            indent // treat each tab as one level
        } else {
            indent / 2
        };

        if let Some((k, v)) = trimmed.split_once('=') {
            let k = k.trim();
            let v = v.trim();
            if spaces <= 1 && current_monitor.is_none() {
                match k {
                    "advancedLayout" => config.workspace_layout.enabled = v == "true",
                    "version" => {
                        config.workspace_layout.version = v.parse().unwrap_or(2);
                    }
                    _ => {}
                }
                continue;
            }
            if let Some(mon) = current_monitor.as_mut() {
                match k {
                    "id" => mon.id = v.to_string(),
                    "name" => mon.name = v.to_string(),
                    "worldX" => mon.world_x = v.parse().unwrap_or(0),
                    "worldY" => mon.world_y = v.parse().unwrap_or(0),
                    "width" => mon.width = v.parse().unwrap_or(0),
                    "height" => mon.height = v.parse().unwrap_or(0),
                    "localX" => mon.local_x = v.parse().unwrap_or(0),
                    "localY" => mon.local_y = v.parse().unwrap_or(0),
                    "scale" => mon.scale = v.parse().unwrap_or(1.0),
                    "dpi" => mon.dpi = v.parse().unwrap_or(96),
                    "layoutWidth" => mon.layout_width = v.parse().unwrap_or(0),
                    "layoutHeight" => mon.layout_height = v.parse().unwrap_or(0),
                    "needsPlacement" => mon.needs_placement = v == "true",
                    _ => {}
                }
            }
            continue;
        }

        if let Some(name) = trimmed.strip_suffix(':') {
            let name = name.trim().to_string();
            if spaces <= 1 {
                // machine block
                flush_monitor(config, &current_machine, &mut current_monitor);
                current_machine = Some(name.clone());
                current_monitor = None;
                if !config
                    .workspace_layout
                    .machines
                    .iter()
                    .any(|m| m.name == name)
                {
                    config.workspace_layout.machines.push(MachineLayout {
                        name,
                        monitors: vec![],
                    });
                }
            } else {
                // monitor block
                flush_monitor(config, &current_machine, &mut current_monitor);
                current_monitor = Some(DisplayRect {
                    id: name.clone(),
                    name,
                    world_x: 0,
                    world_y: 0,
                    width: 0,
                    height: 0,
                    local_x: 0,
                    local_y: 0,
                    scale: 1.0,
                    dpi: 96,
                    layout_width: 0,
                    layout_height: 0,
                    needs_placement: false,
                });
            }
        }
    }

    flush_monitor(config, &current_machine, &mut current_monitor);
}

fn bool_word(v: bool) -> &'static str {
    if v { "true" } else { "false" }
}
