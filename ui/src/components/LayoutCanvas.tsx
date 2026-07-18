import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { DisplayRect, WorkspaceLayout } from "../types";
import {
  MACHINE_COLORS,
  boundsOf,
  computeEdgeHints,
  layoutH,
  layoutW,
  snapMonitorPosition,
} from "../lib/layoutSnap";

export type MonitorRef = {
  machineIndex: number;
  monitorIndex: number;
};

type DragState = {
  ref: MonitorRef;
  originWorldX: number;
  originWorldY: number;
  startClientX: number;
  startClientY: number;
};

export function LayoutCanvas({
  layout,
  selectedMachine,
  selection,
  onSelect,
  onMoveMonitor,
}: {
  layout: WorkspaceLayout;
  selectedMachine: string;
  selection: MonitorRef | null;
  onSelect: (ref: MonitorRef | null) => void;
  onMoveMonitor: (ref: MonitorRef, worldX: number, worldY: number) => void;
}) {
  const svgRef = useRef<SVGSVGElement>(null);
  const [viewBox, setViewBox] = useState({ x: -200, y: -200, w: 2600, h: 1600 });
  const [drag, setDrag] = useState<DragState | null>(null);
  const [panning, setPanning] = useState<{
    startX: number;
    startY: number;
    originX: number;
    originY: number;
  } | null>(null);

  const fit = useCallback(() => {
    const b = boundsOf(layout.machines, 280);
    setViewBox({
      x: b.minX,
      y: b.minY,
      w: Math.max(800, b.maxX - b.minX),
      h: Math.max(600, b.maxY - b.minY),
    });
  }, [layout.machines]);

  useEffect(() => {
    fit();
  }, [fit]);

  const edges = useMemo(() => computeEdgeHints(layout.machines), [layout.machines]);

  const clientToWorld = useCallback(
    (clientX: number, clientY: number) => {
      const svg = svgRef.current;
      if (!svg) return { x: 0, y: 0 };
      const rect = svg.getBoundingClientRect();
      const x = viewBox.x + ((clientX - rect.left) / rect.width) * viewBox.w;
      const y = viewBox.y + ((clientY - rect.top) / rect.height) * viewBox.h;
      return { x, y };
    },
    [viewBox],
  );

  function onWheel(e: React.WheelEvent) {
    e.preventDefault();
    const factor = e.deltaY > 0 ? 1.12 : 1 / 1.12;
    const { x: mx, y: my } = clientToWorld(e.clientX, e.clientY);
    const nw = viewBox.w * factor;
    const nh = viewBox.h * factor;
    setViewBox({
      x: mx - ((mx - viewBox.x) / viewBox.w) * nw,
      y: my - ((my - viewBox.y) / viewBox.h) * nh,
      w: nw,
      h: nh,
    });
  }

  function onPointerDown(e: React.PointerEvent, ref: MonitorRef, mon: DisplayRect) {
    const machine = layout.machines[ref.machineIndex];
    if (!machine || machine.name !== selectedMachine) {
      onSelect(ref);
      return;
    }
    e.currentTarget.setPointerCapture(e.pointerId);
    onSelect(ref);
    setDrag({
      ref,
      originWorldX: mon.worldX,
      originWorldY: mon.worldY,
      startClientX: e.clientX,
      startClientY: e.clientY,
    });
  }

  function onBackgroundDown(e: React.PointerEvent) {
    if (e.button !== 0 && e.button !== 1) return;
    if ((e.target as Element).closest("[data-monitor]")) return;
    e.currentTarget.setPointerCapture(e.pointerId);
    onSelect(null);
    setPanning({
      startX: e.clientX,
      startY: e.clientY,
      originX: viewBox.x,
      originY: viewBox.y,
    });
  }

  function onPointerMove(e: React.PointerEvent) {
    if (panning) {
      const svg = svgRef.current;
      if (!svg) return;
      const rect = svg.getBoundingClientRect();
      const dx = ((e.clientX - panning.startX) / rect.width) * viewBox.w;
      const dy = ((e.clientY - panning.startY) / rect.height) * viewBox.h;
      setViewBox((vb) => ({
        ...vb,
        x: panning.originX - dx,
        y: panning.originY - dy,
      }));
      return;
    }
    if (!drag) return;
    const svg = svgRef.current;
    if (!svg) return;
    const rect = svg.getBoundingClientRect();
    const dx = ((e.clientX - drag.startClientX) / rect.width) * viewBox.w;
    const dy = ((e.clientY - drag.startClientY) / rect.height) * viewBox.h;
    let nx = Math.round(drag.originWorldX + dx);
    let ny = Math.round(drag.originWorldY + dy);
    const machine = layout.machines[drag.ref.machineIndex];
    const mon = machine?.monitors[drag.ref.monitorIndex];
    if (mon) {
      const key = `${machine.name}::${mon.id || mon.name}`;
      const snapped = snapMonitorPosition(mon, nx, ny, layout.machines, key);
      nx = snapped.x;
      ny = snapped.y;
    }
    onMoveMonitor(drag.ref, nx, ny);
  }

  function onPointerUp() {
    setDrag(null);
    setPanning(null);
  }

  return (
    <div className="relative h-full min-h-[320px] w-full overflow-hidden">
      <div className="absolute right-4 top-4 z-20 flex gap-1">
        <button
          type="button"
          className="rounded border border-gray-700 bg-gray-900/80 px-3 py-1.5 text-xs font-medium text-gray-400 shadow-lg backdrop-blur-sm transition-colors hover:bg-gray-800 hover:text-white"
          onClick={fit}
        >
          Fit View
        </button>
      </div>
      <svg
        ref={svgRef}
        className="h-full min-h-[320px] w-full touch-none cursor-grab active:cursor-grabbing"
        viewBox={`${viewBox.x} ${viewBox.y} ${viewBox.w} ${viewBox.h}`}
        onWheel={onWheel}
        onPointerDown={onBackgroundDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onPointerCancel={onPointerUp}
      >
        <defs>
          <pattern id="grid" width="100" height="100" patternUnits="userSpaceOnUse">
            <path
              d="M 100 0 L 0 0 0 100"
              fill="none"
              stroke="#1e293b"
              strokeWidth="2"
            />
          </pattern>
        </defs>
        <rect
          x={viewBox.x - viewBox.w}
          y={viewBox.y - viewBox.h}
          width={viewBox.w * 3}
          height={viewBox.h * 3}
          fill="url(#grid)"
        />

        {edges.map((edge, i) => (
          <line
            key={`e-${i}`}
            x1={edge.x1}
            y1={edge.y1}
            x2={edge.x2}
            y2={edge.y2}
            stroke={edge.ok ? "#34d399" : "#fbbf24"}
            strokeWidth={edge.ok ? 10 : 8}
            strokeLinecap="round"
            opacity={0.85}
          />
        ))}

        {layout.machines.map((machine, mi) =>
          machine.monitors.map((mon, di) => {
            const active = machine.name === selectedMachine;
            const selected =
              selection?.machineIndex === mi && selection?.monitorIndex === di;
            const color =
              MACHINE_COLORS[mi % MACHINE_COLORS.length] ?? "#3d9cf0";
            const w = layoutW(mon);
            const h = layoutH(mon);
            const labelSize = Math.max(18, Math.min(42, w / 18));
            return (
              <g
                key={`${mi}-${di}`}
                data-monitor
                transform={`translate(${mon.worldX}, ${mon.worldY})`}
                opacity={active ? 1 : 0.45}
                onPointerDown={(e) => {
                  e.stopPropagation();
                  onPointerDown(e, { machineIndex: mi, monitorIndex: di }, mon);
                }}
                style={{ cursor: active ? "move" : "pointer" }}
              >
                <rect
                  width={w}
                  height={h}
                  rx={Math.min(24, w / 40)}
                  fill={mon.needsPlacement ? "#f59e0b33" : `${color}33`}
                  stroke={
                    selected
                      ? "#fff"
                      : mon.needsPlacement
                        ? "#f59e0b"
                        : color
                  }
                  strokeWidth={selected ? 14 : 8}
                />
                <text
                  x={24}
                  y={labelSize + 16}
                  fill="#e8eef6"
                  fontSize={labelSize}
                  fontWeight={600}
                >
                  {machine.name}
                </text>
                <text
                  x={24}
                  y={labelSize * 2 + 28}
                  fill="#94a3b8"
                  fontSize={labelSize * 0.75}
                >
                  {mon.name || mon.id}
                  {mon.needsPlacement ? " · place me" : ""}
                </text>
                <text
                  x={24}
                  y={h - 24}
                  fill="#64748b"
                  fontSize={labelSize * 0.65}
                >
                  {w}×{h}
                </text>
              </g>
            );
          }),
        )}
      </svg>
      <div className="border-t border-[var(--border)] px-3 py-1.5 text-[11px] text-[var(--text-muted)]">
        Drag monitors on the selected machine · scroll to zoom · drag background to pan ·
        green edges route, amber edges are close but not abutted
      </div>
    </div>
  );
}
