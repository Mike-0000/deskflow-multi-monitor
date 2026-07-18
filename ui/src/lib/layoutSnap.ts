import type { DisplayRect, MachineLayout } from "../types";

export const EDGE_ABUT_TOLERANCE = 2;
export const SNAP_DISTANCE = 48;

export function layoutW(m: DisplayRect): number {
  return m.layoutWidth && m.layoutWidth > 0 ? m.layoutWidth : m.width || 1920;
}

export function layoutH(m: DisplayRect): number {
  return m.layoutHeight && m.layoutHeight > 0 ? m.layoutHeight : m.height || 1080;
}

function intervalsOverlap(a0: number, a1: number, b0: number, b1: number) {
  return Math.max(a0, b0) < Math.min(a1, b1);
}

/** Port of AdvancedLayoutWidget::snapMonitorPosition (world units). */
export function snapMonitorPosition(
  moving: DisplayRect,
  proposedX: number,
  proposedY: number,
  machines: MachineLayout[],
  movingKey: string,
): { x: number; y: number } {
  const movingW = layoutW(moving);
  const movingH = layoutH(moving);
  const proposedRight = proposedX + movingW;
  const proposedBottom = proposedY + movingH;

  let snappedX = proposedX;
  let snappedY = proposedY;
  let bestDx = SNAP_DISTANCE + 1;
  let bestDy = SNAP_DISTANCE + 1;

  const considerX = (candidateX: number, other: DisplayRect) => {
    if (
      !intervalsOverlap(
        proposedY,
        proposedBottom,
        other.worldY,
        other.worldY + layoutH(other),
      )
    ) {
      return;
    }
    const dx = Math.abs(candidateX - proposedX);
    if (dx < bestDx) {
      bestDx = dx;
      snappedX = candidateX;
    }
  };

  const considerY = (candidateY: number, other: DisplayRect) => {
    if (
      !intervalsOverlap(
        proposedX,
        proposedRight,
        other.worldX,
        other.worldX + layoutW(other),
      )
    ) {
      return;
    }
    const dy = Math.abs(candidateY - proposedY);
    if (dy < bestDy) {
      bestDy = dy;
      snappedY = candidateY;
    }
  };

  for (const machine of machines) {
    for (const other of machine.monitors) {
      const key = `${machine.name}::${other.id || other.name}`;
      if (key === movingKey) continue;

      considerX(other.worldX - movingW, other);
      considerX(other.worldX + layoutW(other), other);
      considerX(other.worldX, other);
      considerX(other.worldX + layoutW(other) - movingW, other);

      considerY(other.worldY - movingH, other);
      considerY(other.worldY + layoutH(other), other);
      considerY(other.worldY, other);
      considerY(other.worldY + layoutH(other) - movingH, other);
    }
  }

  return { x: snappedX, y: snappedY };
}

export type EdgeHint = {
  x1: number;
  y1: number;
  x2: number;
  y2: number;
  ok: boolean;
};

/** Drawable abutting edges between different machines (within tolerance). */
export function computeEdgeHints(machines: MachineLayout[]): EdgeHint[] {
  const hints: EdgeHint[] = [];
  const flats = machines.flatMap((machine) =>
    machine.monitors.map((m) => ({ machine: machine.name, m })),
  );

  for (let i = 0; i < flats.length; i++) {
    for (let j = i + 1; j < flats.length; j++) {
      if (flats[i].machine === flats[j].machine) continue;
      const a = flats[i].m;
      const b = flats[j].m;
      const aw = layoutW(a);
      const ah = layoutH(a);
      const bw = layoutW(b);
      const bh = layoutH(b);

      // Horizontal abutment (a right ↔ b left)
      const gapH = Math.abs(a.worldX + aw - b.worldX);
      const yOverlap =
        Math.max(a.worldY, b.worldY) < Math.min(a.worldY + ah, b.worldY + bh);
      if (yOverlap && gapH <= SNAP_DISTANCE) {
        const y0 = Math.max(a.worldY, b.worldY);
        const y1 = Math.min(a.worldY + ah, b.worldY + bh);
        hints.push({
          x1: a.worldX + aw,
          y1: y0,
          x2: a.worldX + aw,
          y2: y1,
          ok: gapH <= EDGE_ABUT_TOLERANCE,
        });
      }

      // Vertical abutment (a bottom ↔ b top)
      const gapV = Math.abs(a.worldY + ah - b.worldY);
      const xOverlap =
        Math.max(a.worldX, b.worldX) < Math.min(a.worldX + aw, b.worldX + bw);
      if (xOverlap && gapV <= SNAP_DISTANCE) {
        const x0 = Math.max(a.worldX, b.worldX);
        const x1 = Math.min(a.worldX + aw, b.worldX + bw);
        hints.push({
          x1: x0,
          y1: a.worldY + ah,
          x2: x1,
          y2: a.worldY + ah,
          ok: gapV <= EDGE_ABUT_TOLERANCE,
        });
      }
    }
  }
  return hints;
}

export function boundsOf(
  machines: MachineLayout[],
  padding = 320,
): { minX: number; minY: number; maxX: number; maxY: number } {
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  let any = false;
  for (const machine of machines) {
    for (const m of machine.monitors) {
      any = true;
      minX = Math.min(minX, m.worldX);
      minY = Math.min(minY, m.worldY);
      maxX = Math.max(maxX, m.worldX + layoutW(m));
      maxY = Math.max(maxY, m.worldY + layoutH(m));
    }
  }
  if (!any) {
    return { minX: -padding, minY: -padding, maxX: 1920 + padding, maxY: 1080 + padding };
  }
  return {
    minX: minX - padding,
    minY: minY - padding,
    maxX: maxX + padding,
    maxY: maxY + padding,
  };
}

export const MACHINE_COLORS = [
  "#3d9cf0",
  "#34d399",
  "#f59e0b",
  "#a78bfa",
  "#f472b6",
  "#22d3ee",
];
