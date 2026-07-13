#!/usr/bin/env python3
import cairo
import math

# Create PNG from the icon design
def draw_icon(surface, width, height):
    ctx = cairo.Context(surface)
    ctx.scale(width / 512, height / 512)
    
    # Background gradient circle
    gradient = cairo.LinearGradient(0, 0, 512, 512)
    gradient.add_color_stop_rgb(0, 0.99, 0.42, 0.21)  # #FF6B35
    gradient.add_color_stop_rgb(0.5, 0.97, 0.58, 0.12)  # #F7931E
    gradient.add_color_stop_rgb(1, 1.0, 0.82, 0.25)     # #FFD23F
    
    ctx.arc(256, 256, 240, 0, 2 * math.pi)
    ctx.set_source(gradient)
    ctx.fill()
    
    # Inner glow ring
    glow = cairo.LinearGradient(256, 0, 256, 512)
    glow.add_color_stop_rgba(0, 1, 1, 1, 0.3)
    glow.add_color_stop_rgba(0.5, 0.99, 0.42, 0.21, 0.1)
    glow.add_color_stop_rgba(1, 0.99, 0.42, 0.21, 0)
    ctx.arc(256, 256, 220, 0, 2 * math.pi)
    ctx.set_source(glow)
    ctx.set_line_width(4)
    ctx.stroke()
    
    # Anvil - base
    ctx.save()
    ctx.translate(256, 256)
    ctx.scale(1.2, 1.2)
    
    # Anvil base
    ctx.move_to(-60, 20)
    ctx.line_to(60, 20)
    ctx.line_to(50, 40)
    ctx.line_to(-50, 40)
    ctx.close_path()
    ctx.set_source_rgb(0.1, 0.1, 0.18)  # #1A1A2E
    ctx.fill_preserve()
    ctx.set_source_rgb(0.99, 0.42, 0.21)
    ctx.set_line_width(2)
    ctx.stroke()
    
    # Anvil horn
    ctx.move_to(-60, 20)
    ctx.line_to(-80, -30)
    ctx.line_to(-70, -30)
    ctx.line_to(-50, 20)
    ctx.close_path()
    ctx.set_source_rgb(0.18, 0.18, 0.27)  # #2D2D44
    ctx.fill_preserve()
    ctx.set_source_rgb(0.99, 0.42, 0.21)
    ctx.stroke()
    
    # Anvil face/top
    ctx.move_to(-80, -30)
    ctx.line_to(80, -30)
    ctx.line_to(70, -50)
    ctx.line_to(-70, -50)
    ctx.close_path()
    ctx.set_source_rgb(0.1, 0.1, 0.18)
    ctx.fill_preserve()
    ctx.set_source_rgb(0.99, 0.42, 0.21)
    ctx.stroke()
    
    # Anvil heel
    ctx.move_to(50, 20)
    ctx.line_to(80, -30)
    ctx.line_to(70, -30)
    ctx.line_to(50, 40)
    ctx.close_path()
    ctx.set_source_rgb(0.18, 0.18, 0.27)
    ctx.fill_preserve()
    ctx.set_source_rgb(0.99, 0.42, 0.21)
    ctx.stroke()
    
    # Hammer
    ctx.save()
    ctx.translate(-30, -80)
    ctx.rotate(-30 * math.pi / 180)
    ctx.rectangle(-10, -20, 20, 40)
    ctx.set_source_rgb(0.75, 0.75, 0.75)  # #C0C0C0
    ctx.fill_preserve()
    ctx.set_source_rgb(0.53, 0.53, 0.53)  # #888
    ctx.set_line_width(1.5)
    ctx.stroke()
    
    # Hammer head
    ctx.move_to(-20, -20)
    ctx.line_to(20, -20)
    ctx.line_to(15, -35)
    ctx.line_to(-15, -35)
    ctx.close_path()
    ctx.set_source_rgb(0.29, 0.29, 0.35)  # #4A4A5A
    ctx.fill()
    ctx.restore()
    
    # Sparks
    for i, (x, y, r, color, phase) in enumerate([
        (55, -25, 3, (1.0, 0.82, 0.25), 0),
        (65, -15, 2, (0.99, 0.42, 0.21), 0.5),
        (50, -35, 2.5, (0.97, 0.58, 0.12), 0.3),
    ]):
        ctx.arc(x, y, r, 0, 2 * math.pi)
        ctx.set_source_rgb(*color)
        ctx.fill()
    
    ctx.restore()
    
    # FORGE text
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(48)
    ctx.move_to(256 - 80, 440)
    ctx.set_source_rgb(0.1, 0.1, 0.18)
    ctx.set_line_width(2)
    ctx.text_path("FORGE")
    ctx.stroke_preserve()
    ctx.set_source_rgb(1, 1, 1)
    ctx.fill()

# Generate PNG at multiple sizes
sizes = [16, 24, 32, 48, 64, 128, 256, 512]
for size in sizes:
    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, size, size)
    draw_icon(surface, size, size)
    surface.write_to_png(f"/home/nohat1337/c++-fork/assets/icon-{size}.png")
    print(f"Generated icon-{size}.png")

# Generate ICO (Windows icon) - ICO can contain multiple sizes
# For simplicity, create a single 256x256 ICO
surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, 256, 256)
draw_icon(surface, 256, 256)
surface.write_to_png("/home/nohat1337/c++-fork/assets/icon.png")
print("Generated icon.png (256x256)")

# Also create a simple 256x256 for Linux
surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, 256, 256)
draw_icon(surface, 256, 256)
surface.write_to_png("/home/nohat1337/c++-fork/assets/forge-icon.png")
print("Generated forge-icon.png")