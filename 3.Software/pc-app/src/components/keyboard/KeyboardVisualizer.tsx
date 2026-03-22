interface KeyDef {
  x: number;
  y: number;
  w: number;
  h: number;
  label: string;
}

// Simplified 75% keyboard layout (14 columns, 5 rows + function row)
const UNIT = 36;
const GAP = 2;

function key(
  col: number,
  row: number,
  label: string,
  w = 1,
  h = 1
): KeyDef {
  return {
    x: col * (UNIT + GAP),
    y: row * (UNIT + GAP),
    w: w * UNIT + (w - 1) * GAP,
    h: h * UNIT + (h - 1) * GAP,
    label,
  };
}

const KEYS: KeyDef[] = [
  // Row 0: Esc F1-F12 Del
  key(0, 0, "Esc"),
  ...Array.from({ length: 12 }, (_, i) => key(i + 1.5, 0, `F${i + 1}`)),
  key(14, 0, "Del"),

  // Row 1: ` 1-0 - = Bksp
  key(0, 1, "`"),
  ...["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"].map((l, i) =>
    key(i + 1, 1, l)
  ),
  key(11, 1, "-"),
  key(12, 1, "="),
  key(13, 1, "Bksp", 2),

  // Row 2: Tab Q-P [ ] Backslash
  key(0, 2, "Tab", 1.5),
  ...["Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"].map((l, i) =>
    key(i + 1.5, 2, l)
  ),
  key(11.5, 2, "["),
  key(12.5, 2, "]"),
  key(13.5, 2, "\\", 1.5),

  // Row 3: Caps A-L ; ' Enter
  key(0, 3, "Caps", 1.75),
  ...["A", "S", "D", "F", "G", "H", "J", "K", "L"].map((l, i) =>
    key(i + 1.75, 3, l)
  ),
  key(10.75, 3, ";"),
  key(11.75, 3, "'"),
  key(12.75, 3, "Enter", 2.25),

  // Row 4: Shift Z-M , . / Shift Up
  key(0, 4, "Shift", 2.25),
  ...["Z", "X", "C", "V", "B", "N", "M"].map((l, i) =>
    key(i + 2.25, 4, l)
  ),
  key(9.25, 4, ","),
  key(10.25, 4, "."),
  key(11.25, 4, "/"),
  key(12.25, 4, "Shift", 1.75),
  key(14, 4, "Up"),

  // Row 5: Ctrl Win Alt Space Alt Fn Ctrl Left Down Right
  key(0, 5, "Ctrl", 1.25),
  key(1.25, 5, "Win", 1.25),
  key(2.5, 5, "Alt", 1.25),
  key(3.75, 5, "Space", 6.25),
  key(10, 5, "Alt"),
  key(11, 5, "Fn"),
  key(12, 5, "Ctrl"),
  key(13, 5, "Left"),
  key(14, 5, "Down"),
  key(15, 5, "Right"),
];

const SVG_W = 16 * UNIT + 15 * GAP;
const SVG_H = 6 * UNIT + 5 * GAP;

interface Props {
  activeColor?: string;
}

export default function KeyboardVisualizer({ activeColor = "#6366f1" }: Props) {
  return (
    <svg
      viewBox={`-4 -4 ${SVG_W + 8} ${SVG_H + 8}`}
      className="w-full"
      style={{ maxWidth: 700 }}
    >
      {KEYS.map((k, i) => (
        <g key={i}>
          <rect
            x={k.x}
            y={k.y}
            width={k.w}
            height={k.h}
            rx={4}
            fill={activeColor}
            opacity={0.15}
            stroke={activeColor}
            strokeWidth={1}
            strokeOpacity={0.4}
          />
          <text
            x={k.x + k.w / 2}
            y={k.y + k.h / 2 + 4}
            textAnchor="middle"
            fill="currentColor"
            fontSize={k.label.length > 2 ? 8 : 10}
            fontFamily="Inter, sans-serif"
            opacity={0.7}
          >
            {k.label}
          </text>
        </g>
      ))}
    </svg>
  );
}
