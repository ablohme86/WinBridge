import subprocess
from pathlib import Path

ICONS = {}

# 1. UNINSTALL: Software box with red badge
ICONS["uninstall"] = """<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <!-- Box Top Face -->
  <polygon points="8,1.2 13.5,3.9 8,6.6 2.5,3.9" fill="#93c5fd" stroke="#0f172a" stroke-width="0.8" stroke-linejoin="round"/>
  <!-- Box Left Face -->
  <polygon points="2.5,3.9 8,6.6 8,13.5 2.5,10.8" fill="#3b82f6" stroke="#0f172a" stroke-width="0.8" stroke-linejoin="round"/>
  <!-- Box Right Face -->
  <polygon points="8,6.6 13.5,3.9 13.5,10.8 8,13.5" fill="#1d4ed8" stroke="#0f172a" stroke-width="0.8" stroke-linejoin="round"/>
  <line x1="8" y1="6.6" x2="8" y2="13.5" stroke="#172554" stroke-width="0.8"/>
  <!-- Package tape / seal -->
  <polygon points="6.8,2.2 9.2,3.3 9.2,5.5 6.8,4.4" fill="#fbbf24" opacity="0.9"/>
  <!-- Red Badge for removal -->
  <circle cx="11.5" cy="11.5" r="4.2" fill="#dc2626" stroke="#ffffff" stroke-width="1"/>
  <circle cx="11.5" cy="11.5" r="4.2" fill="none" stroke="#7f1d1d" stroke-width="0.8"/>
  <line x1="9.5" y1="9.5" x2="13.5" y2="13.5" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round"/>
  <line x1="13.5" y1="9.5" x2="9.5" y2="13.5" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round"/>
</svg>"""

# 2. SHOW FILES: Manila folder with white document
ICONS["show-files"] = """<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <!-- Back folder -->
  <path d="M1.5 2.8 C1.5 2.1 2.1 1.5 2.8 1.5 L6.2 1.5 L7.8 3 L13.2 3 C13.9 3 14.5 3.6 14.5 4.3 L14.5 12.7 C14.5 13.4 13.9 14 13.2 14 L2.8 14 C2.1 14 1.5 13.4 1.5 12.7 Z" fill="#d97706" stroke="#78350f" stroke-width="0.8" stroke-linejoin="round"/>
  <!-- Document -->
  <rect x="3.5" y="2.8" width="9" height="7.2" rx="0.6" fill="#ffffff" stroke="#94a3b8" stroke-width="0.6"/>
  <line x1="5.2" y1="5" x2="10.8" y2="5" stroke="#38bdf8" stroke-width="0.9" stroke-linecap="round"/>
  <line x1="5.2" y1="7" x2="9.8" y2="7" stroke="#cbd5e1" stroke-width="0.9" stroke-linecap="round"/>
  <!-- Front folder flap -->
  <path d="M1.2 6.5 L3.8 6.5 C4.5 6.5 5.1 6.9 5.4 7.5 L6.1 9 L13.5 9 C14.2 9 14.8 9.6 14.8 10.3 L14 13.2 C13.8 13.8 13.2 14.2 12.6 14.2 L2.2 14.2 C1.5 14.2 1 13.6 1 12.9 Z" fill="#fbbf24" stroke="#78350f" stroke-width="0.8" stroke-linejoin="round"/>
  <!-- Highlight on flap -->
  <path d="M1.5 7.5 L12.8 7.5" stroke="#fef08a" stroke-width="0.7" opacity="0.9"/>
</svg>"""

# 3. SHORTCUT: Window with blue shortcut arrow badge
ICONS["shortcut"] = """<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <rect x="2.5" y="1" width="12" height="13.5" rx="1" fill="#f8fafc" stroke="#1e293b" stroke-width="0.8"/>
  <rect x="2.9" y="1.4" width="11.2" height="3.2" fill="#2563eb"/>
  <circle cx="12.8" cy="3" r="0.7" fill="#ffffff"/>
  <line x1="5" y1="6.5" x2="12.5" y2="6.5" stroke="#94a3b8" stroke-width="0.8" stroke-linecap="round"/>
  <line x1="5" y1="8.5" x2="12.5" y2="8.5" stroke="#94a3b8" stroke-width="0.8" stroke-linecap="round"/>
  <line x1="7.5" y1="10.5" x2="12.5" y2="10.5" stroke="#cbd5e1" stroke-width="0.8" stroke-linecap="round"/>
  <line x1="7.5" y1="12.5" x2="11.5" y2="12.5" stroke="#cbd5e1" stroke-width="0.8" stroke-linecap="round"/>
  <!-- Shortcut badge -->
  <rect x="0.5" y="8.5" width="7" height="7" rx="1" fill="#ffffff" stroke="#0f172a" stroke-width="0.8"/>
  <path d="M2.2 13.5 C2.2 11.2 3.2 10.2 5.5 10.2" fill="none" stroke="#0284c7" stroke-width="1.4" stroke-linecap="round"/>
  <polygon points="3.6,9.2 6.8,9.6 5.8,12.8" fill="#0284c7"/>
</svg>"""

# 4. KILL: Octagon stop danger sign
ICONS["kill"] = """<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <polygon points="5,1 11,1 15,5 15,11 11,15 5,15 1,11 1,5" fill="#dc2626" stroke="#450a0a" stroke-width="0.9" stroke-linejoin="round"/>
  <polyline points="2.2,5 5,2.2 11,2.2 13.8,5" fill="none" stroke="#f87171" stroke-width="0.8"/>
  <line x1="5" y1="5" x2="11" y2="11" stroke="#ffffff" stroke-width="1.9" stroke-linecap="square"/>
  <line x1="11" y1="5" x2="5" y2="11" stroke="#ffffff" stroke-width="1.9" stroke-linecap="square"/>
</svg>"""

# 5. REFRESH: Bolder, vibrant circular refresh arrows with gradient/highlight
ICONS["refresh"] = """<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <!-- Top-right arc -->
  <path d="M 8 2.2 A 5.8 5.8 0 0 1 13.8 8" fill="none" stroke="#0284c7" stroke-width="2.2" stroke-linecap="round"/>
  <path d="M 8 2.2 A 5.8 5.8 0 0 1 13.8 8" fill="none" stroke="#38bdf8" stroke-width="1.2" stroke-linecap="round"/>
  <polygon points="10.8,0.8 15.2,1.8 14.2,6" fill="#0284c7" stroke="#0369a1" stroke-width="0.6"/>
  <!-- Bottom-left arc -->
  <path d="M 8 13.8 A 5.8 5.8 0 0 1 2.2 8" fill="none" stroke="#0284c7" stroke-width="2.2" stroke-linecap="round"/>
  <path d="M 8 13.8 A 5.8 5.8 0 0 1 2.2 8" fill="none" stroke="#38bdf8" stroke-width="1.2" stroke-linecap="round"/>
  <polygon points="5.2,15.2 0.8,14.2 1.8,10" fill="#0284c7" stroke="#0369a1" stroke-width="0.6"/>
</svg>"""

# 6. DRIVE-C: Classic Windows 95/XP hard disk
ICONS["drive-c"] = """<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <!-- Outer chassis -->
  <rect x="1" y="3" width="14" height="10" rx="1.5" fill="#94a3b8" stroke="#1e293b" stroke-width="0.8"/>
  <!-- Metal plate cover -->
  <rect x="2" y="4" width="12" height="6.5" rx="0.5" fill="#e2e8f0" stroke="#64748b" stroke-width="0.5"/>
  <!-- Platter spindle -->
  <circle cx="5.5" cy="7.2" r="2.2" fill="#cbd5e1" stroke="#94a3b8" stroke-width="0.5"/>
  <circle cx="5.5" cy="7.2" r="0.8" fill="#475569"/>
  <!-- C: text badge -->
  <text x="11" y="8.8" font-family="'Segoe UI', Arial, sans-serif" font-weight="900" font-size="4.5" fill="#1d4ed8" text-anchor="middle">C:</text>
  <!-- Green LED light -->
  <circle cx="3" cy="11.4" r="0.9" fill="#22c55e" stroke="#15803d" stroke-width="0.4"/>
</svg>"""

# 7. SETTINGS: Metallic mechanical gears with crisp highlights
ICONS["settings"] = """<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <path d="M7 1.5 L9 1.5 L9.3 3.3 C9.9 3.5 10.4 3.8 10.9 4.2 L12.7 3.5 L14 4.8 L13.3 6.6 C13.7 7.1 14 7.6 14.2 8.2 L16 8.5 L16 10.5 L14.2 10.8 C14 11.4 13.7 11.9 13.3 12.4 L14 14.2 L12.7 15.5 L10.9 14.8 C10.4 15.2 9.9 15.5 9.3 15.7 L9 17.5 L7 17.5 L6.7 15.7 C6.1 15.5 5.6 15.2 5.1 14.8 L3.3 15.5 L2 14.2 L2.7 12.4 C2.3 11.9 2 11.4 1.8 10.8 L0 10.5 L0 8.5 L1.8 8.2 C2 7.6 2.3 7.1 2.7 6.6 L2 4.8 L3.3 3.5 L5.1 4.2 C5.6 3.8 6.1 3.5 6.7 3.3 Z" transform="translate(0.5, 0) scale(0.88)" fill="#94a3b8" stroke="#1e293b" stroke-width="0.8" stroke-linejoin="round"/>
  <!-- Highlight ring -->
  <circle cx="8" cy="8.3" r="3.2" fill="#cbd5e1"/>
  <!-- Center bore -->
  <circle cx="8" cy="8.3" r="2.2" fill="#334155" stroke="#1e293b" stroke-width="0.7"/>
  <circle cx="8" cy="8.3" r="1.1" fill="#1e293b"/>
</svg>"""

# 8. ABOUT: Deep blue circle with info 'i'
ICONS["about"] = """<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <circle cx="8" cy="8" r="7" fill="#2563eb" stroke="#0f172a" stroke-width="0.9"/>
  <!-- Highlight ring -->
  <path d="M2.8 5 A 5.8 5.8 0 0 1 13.2 5" fill="none" stroke="#93c5fd" stroke-width="0.8"/>
  <circle cx="8" cy="4.5" r="1.1" fill="#ffffff"/>
  <rect x="7" y="7" width="2" height="5.5" rx="0.5" fill="#ffffff"/>
  <rect x="6.2" y="7" width="1.5" height="1" rx="0.3" fill="#ffffff"/>
  <rect x="6.2" y="11.5" width="3.6" height="1" rx="0.3" fill="#ffffff"/>
</svg>"""

# 9. DOWNLOAD: Download arrow into tray
ICONS["download"] = """<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <!-- Down arrow -->
  <path d="M8 1.5 L8 9.5" stroke="#16a34a" stroke-width="2.2" stroke-linecap="round"/>
  <polygon points="4.5,8 11.5,8 8,12" fill="#16a34a" stroke="#14532d" stroke-width="0.6"/>
  <!-- Tray -->
  <path d="M2 10 L2 14 C2 14.5 2.5 15 3 15 L13 15 C13.5 15 14 14.5 14 14 L14 10" fill="none" stroke="#1e293b" stroke-width="1.5" stroke-linecap="round"/>
</svg>"""

# 10. SAVE: 3.5" retro floppy disk
ICONS["save"] = """<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <path d="M2 1.5 L12 1.5 L14.5 4 L14.5 14 C14.5 14.5 14 15 13.5 15 L2.5 15 C2 15 1.5 14.5 1.5 14 L1.5 2 C1.5 1.5 2 1.5 2 1.5 Z" fill="#2563eb" stroke="#0f172a" stroke-width="0.8" stroke-linejoin="round"/>
  <!-- Metal shutter on top -->
  <rect x="4.5" y="1.5" width="6.5" height="5" rx="0.5" fill="#cbd5e1" stroke="#475569" stroke-width="0.5"/>
  <rect x="6" y="2.5" width="1.5" height="3" fill="#1e293b"/>
  <!-- Paper label on bottom -->
  <rect x="3.5" y="8.5" width="9" height="6.5" rx="0.5" fill="#f8fafc" stroke="#94a3b8" stroke-width="0.5"/>
  <line x1="5" y1="10.5" x2="11" y2="10.5" stroke="#3b82f6" stroke-width="0.8" stroke-linecap="round"/>
  <line x1="5" y1="12.5" x2="9" y2="12.5" stroke="#94a3b8" stroke-width="0.8" stroke-linecap="round"/>
</svg>"""

out_dir = Path("manager/icons")
out_dir.mkdir(parents=True, exist_ok=True)
svg_dir = out_dir / "svg"
svg_dir.mkdir(parents=True, exist_ok=True)

for name, svg in ICONS.items():
    svg_file = svg_dir / f"{name}.svg"
    svg_file.write_text(svg)
    png_16 = out_dir / f"{name}.png"
    png_32 = out_dir / f"{name}@2x.png"
    subprocess.run(["rsvg-convert", "-w", "16", "-h", "16", str(svg_file), "-o", str(png_16)], check=True)
    subprocess.run(["rsvg-convert", "-w", "32", "-h", "32", str(svg_file), "-o", str(png_32)], check=True)

# Generate preview composite
from PIL import Image, ImageDraw

icons_list = list(ICONS.keys())
w = len(icons_list) * 55 + 20
h = 160
img = Image.new('RGBA', (w, h), (44, 47, 53, 255))
draw = ImageDraw.Draw(img)
draw.rectangle([0, 0, w, h//2], fill=(192, 192, 192, 255))

for i, name in enumerate(icons_list):
    p32 = Image.open(f'manager/icons/{name}@2x.png')
    x = 15 + i * 55
    img.paste(p32, (x, 24), p32)
    img.paste(p32, (x, 104), p32)

img.save('/tmp/icons_preview2.png')
print('Rendered updated icons and preview')
