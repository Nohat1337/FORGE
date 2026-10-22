package {{PACKAGE}}

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.rotate
import kotlin.math.cos
import kotlin.math.sin

/**
 * The wallpaper behind the glass.
 *
 * The liquid lens refracts whatever is painted behind it, so the backdrop
 * needs high frequency detail (orbs, grid, specks) - a flat gradient would
 * make the refraction invisible.
 */
@Composable
fun ForgeWallpaper(modifier: Modifier = Modifier) {
    var phase by remember { mutableFloatStateOf(0f) }
    LaunchedEffect(Unit) {
        while (true) {
            kotlinx.coroutines.delay(16)
            phase += 0.01f
        }
    }

    Canvas(modifier) {
        drawRect(
            Brush.verticalGradient(
                listOf(Color(0xFF1C203A), Color(0xFF0B0E18))
            ),
            size = size
        )
        drawOrb(Offset(size.width * 0.26f, size.height * 0.32f), size.minDimension * 0.55f, phase)
        drawOrb(Offset(size.width * 0.74f, size.height * 0.70f), size.minDimension * 0.50f, phase * 1.3f)
        drawOrb(Offset(size.width * 0.56f, size.height * 0.12f), size.minDimension * 0.38f, phase * 0.7f)
        drawGrid()
        drawSpecks()
        drawGlowDots(phase)
    }
}

private fun DrawScope.drawOrb(center: Offset, radius: Float, phase: Float) {
    val cx = center.x + cos(phase) * radius * 0.05f
    val cy = center.y + sin(phase * 0.8f) * radius * 0.05f
    drawCircle(
        Brush.radialGradient(
            colors = listOf(Color(0x6633AAFF), Color(0x0033AAFF)),
            center = Offset(cx, cy),
            radius = radius
        ),
        radius = radius,
        center = Offset(cx, cy)
    )
}

private fun DrawScope.drawGrid() {
    val step = 64f
    var x = 0f
    while (x < size.width) {
        drawLine(
            Color.White.copy(alpha = 0.10f),
            Offset(x, 0f),
            Offset(x, size.height),
            strokeWidth = 1.5f
        )
        x += step
    }
    var y = 0f
    while (y < size.height) {
        drawLine(
            Color.White.copy(alpha = 0.10f),
            Offset(0f, y),
            Offset(size.width, y),
            strokeWidth = 1.5f
        )
        y += step
    }
    // a second, coarser grid gives the lens longer wavelength detail
    var x2 = step / 2f
    while (x2 < size.width) {
        drawLine(
            Color(0xFF6AA9FF).copy(alpha = 0.07f),
            Offset(x2, 0f),
            Offset(x2, size.height),
            strokeWidth = 1f
        )
        x2 += step
    }
}

/** Bright points with a halo: the strongest detail the lens can bend. */
private fun DrawScope.drawGlowDots(phase: Float) {
    val dots = 14
    for (i in 0 until dots) {
        val h = hash(i * 7 + 3)
        val x = h[0] * size.width
        val y = (h[1] * size.height + sin(phase * 0.4f + i) * 18f)
        val r = 2.5f + h[2] * 3.5f
        drawCircle(
            Brush.radialGradient(
                colors = listOf(Color(0xCCBFE3FF), Color(0x0033AAFF)),
                center = Offset(x, y),
                radius = r * 6f
            ),
            radius = r * 6f,
            center = Offset(x, y)
        )
        drawCircle(Color.White.copy(alpha = 0.85f), radius = r, center = Offset(x, y))
    }
}

private fun DrawScope.drawSpecks() {
    var i = 0
    while (i < 220) {
        val h = hash(i)
        val x = (h[0] * size.width).toFloat()
        val y = (h[1] * size.height).toFloat()
        val r = 0.8f + h[2] * 1.8f
        drawCircle(Color.White.copy(alpha = 0.10f + h[2] * 0.22f), radius = r, center = Offset(x, y))
        i++
    }
}

private fun hash(i: Int): FloatArray {
    var x = i * 0x9E3779B1.toInt()
    x = x xor (x ushr 15)
    x *= 0x85EBCA6B.toInt()
    x = x xor (x ushr 13)
    val v = (x and 0xFFFFFF) / 16777215f
    val v2 = ((i * 2654435761u.toInt()) and 0xFFFFFF) / 16777215f
    val v3 = ((i * 40503) and 0xFF) / 255f
    return floatArrayOf(v, v2, v3)
}
