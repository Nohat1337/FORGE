package {{PACKAGE}}

// ============================================================
// Extra liquid widgets for the Forge GUI, built on the Kyant0
// backdrop library.
//
// Everything here is a glass surface: the widget samples the layer
// behind it (the wallpaper), blurs it, refracts it through a lens
// with chromatic dispersion and catches an ambient specular
// highlight on the rim.
// ============================================================

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.draggable2D
import androidx.compose.foundation.gestures.rememberDraggable2DState
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.withTransform
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.kyant.backdrop.Backdrop
import com.kyant.backdrop.drawBackdrop
import com.kyant.backdrop.effects.blur
import com.kyant.backdrop.effects.lens
import com.kyant.backdrop.effects.vibrancy
import com.kyant.backdrop.highlight.Highlight
import com.kyant.backdrop.shadow.InnerShadow
import com.kyant.backdrop.shadow.Shadow
import com.kyant.shapes.Capsule
import com.kyant.shapes.RoundedRectangle

private val GlassWhite = Color(0xFF0A84FF)
private val GlassText = Color.White

/** A glass surface with the library's full material stack. */
@Composable
fun LiquidSurface(
    backdrop: Backdrop,
    modifier: Modifier = Modifier,
    radius: androidx.compose.ui.unit.Dp = 18f.dp,
    blurRadius: androidx.compose.ui.unit.Dp = 6f.dp,
    lensHeight: androidx.compose.ui.unit.Dp = 10f.dp,
    lensAmount: androidx.compose.ui.unit.Dp = 20f.dp,
    dispersion: Boolean = true,
    surfaceAlpha: Float = 0.10f,
    content: @Composable () -> Unit
) {
    Box(
        modifier.drawBackdrop(
            backdrop = backdrop,
            shape = { RoundedRectangle(radius) },
            effects = {
                vibrancy()
                blur(blurRadius.toPx())
                lens(
                    lensHeight.toPx(),
                    lensAmount.toPx(),
                    depthEffect = true,
                    chromaticAberration = dispersion
                )
            },
            highlight = {
                Highlight.Ambient.copy(width = 1f.dp, blurRadius = 0.5f.dp)
            },
            shadow = {
                Shadow(
                    radius = 18f.dp,
                    offset = DpOffset(0f.dp, 6f.dp),
                    color = Color.Black.copy(alpha = 0.22f)
                )
            },
            innerShadow = {
                InnerShadow(
                    radius = 12f.dp,
                    offset = DpOffset(0f.dp, 2f.dp),
                    color = Color.Black.copy(alpha = 0.10f)
                )
            },
            onDrawSurface = {
                drawRect(Color.White.copy(alpha = surfaceAlpha))
            }
        ),
        content = { content() }
    )
}

/**
 * The specular rim every round lens needs: a bright ring of light on the
 * edge of the glass, plus the soft blob of light a glass ball catches, so
 * the disc reads as a lens instead of a hole.
 */
@Composable
fun GlassRim(modifier: Modifier = Modifier, width: androidx.compose.ui.unit.Dp = 3f.dp) {
    Box(
        modifier.drawBehind {
            val radius = size.minDimension / 2f - width.toPx() / 2f
            drawCircle(
                brush = Brush.verticalGradient(
                    colors = listOf(
                        Color.White.copy(alpha = 0.75f),
                        Color.White.copy(alpha = 0.08f),
                        Color.White.copy(alpha = 0.45f)
                    ),
                    startY = 0f,
                    endY = size.height
                ),
                radius = radius,
                style = androidx.compose.ui.graphics.drawscope.Stroke(width = width.toPx())
            )
            val blob = androidx.compose.ui.geometry.Offset(
                size.width * 0.34f,
                size.height * 0.28f
            )
            drawCircle(
                brush = Brush.radialGradient(
                    colors = listOf(
                        Color.White.copy(alpha = 0.28f),
                        Color.White.copy(alpha = 0.0f)
                    ),
                    center = blob,
                    radius = size.minDimension * 0.42f
                ),
                radius = size.minDimension * 0.42f,
                center = blob
            )
        }
    )
}

/** A round glass disc with a specular rim: the base of every dial. */
@Composable
fun LiquidDisc(
    backdrop: Backdrop,
    modifier: Modifier = Modifier,
    zoom: Float = 1f,
    content: @Composable () -> Unit
) {
    Box(modifier.clip(CircleShape)) {
        // The glass body. The zoom is applied to the *sampling* of the
        // backdrop (onDrawBackdrop), not to the node itself, so the
        // refraction, the specular rim and the shadow all stay in the
        // visible space of the disc and it really looks like a lens.
        Box(
            Modifier
                .fillMaxSize()
                .drawBackdrop(
                    backdrop = backdrop,
                    shape = { CircleShape },
                    effects = {
                        vibrancy()
                        blur(0.5f.dp.toPx())
                        lens(
                            16f.dp.toPx(),
                            30f.dp.toPx(),
                            depthEffect = true,
                            chromaticAberration = true
                        )
                    },
                    highlight = {
                        Highlight.Ambient.copy(width = 1.4f.dp, blurRadius = 0.7f.dp)
                    },
                    shadow = {
                        Shadow(
                            radius = 24f.dp,
                            offset = DpOffset(0f.dp, 10f.dp),
                            color = Color.Black.copy(alpha = 0.32f)
                        )
                    },
                    innerShadow = {
                        InnerShadow(
                            radius = 18f.dp,
                            offset = DpOffset(0f.dp, 5f.dp),
                            color = Color.Black.copy(alpha = 0.20f)
                        )
                    },
                    onDrawBackdrop = { drawBackdrop ->
                        if (zoom != 1f) {
                            withTransform({ scale(zoom, zoom) }, drawBackdrop)
                        } else {
                            drawBackdrop()
                        }
                    },
                    onDrawSurface = { drawCircle(Color.White.copy(alpha = 0.05f)) }
                )
        )
        GlassRim(Modifier.fillMaxSize())
        content()
    }
}

/**
 * A frosted tab bar: a glass capsule with a sliding lens indicator,
 * the bar every mobile app parks at the bottom of the screen.
 */
@Composable
fun LiquidTabBar(
    items: List<String>,
    selectedIndex: Int,
    onSelect: (Int) -> Unit,
    backdrop: Backdrop,
    modifier: Modifier = Modifier,
    tint: Color = GlassWhite
) {
    LiquidSurface(
        backdrop = backdrop,
        modifier = modifier,
        radius = 30f.dp,
        blurRadius = 14f.dp,
        lensHeight = 14f.dp,
        lensAmount = 26f.dp,
        surfaceAlpha = 0.12f
    ) {
        if (items.isEmpty()) return@LiquidSurface
        val index = selectedIndex.coerceIn(0, items.size - 1)
        BoxWithConstraints(Modifier.fillMaxSize()) {
            val itemWidth = maxWidth / items.size
            // the indicator: a glass pill that slides to the active tab
            Box(
                Modifier
                    .graphicsLayer {
                        translationX = itemWidth.toPx() * index
                    }
                    .width(itemWidth)
                    .fillMaxHeight()
                    .padding(horizontal = 8f.dp, vertical = 6f.dp)
                    .drawBackdrop(
                        backdrop = backdrop,
                        shape = { Capsule() },
                        effects = {
                            vibrancy()
                            blur(4f.dp.toPx())
                            lens(14f.dp.toPx(), 24f.dp.toPx(), chromaticAberration = true)
                        },
                        highlight = { Highlight.Ambient },
                        shadow = {
                            Shadow(
                                radius = 14f.dp,
                                offset = DpOffset(0f.dp, 5f.dp),
                                color = Color.Black.copy(alpha = 0.28f)
                            )
                        },
                        innerShadow = {
                            InnerShadow(
                                radius = 8f.dp,
                                offset = DpOffset(0f.dp, 2f.dp),
                                color = Color.Black.copy(alpha = 0.16f)
                            )
                        },
                        onDrawSurface = {
                            drawRect(tint.copy(alpha = 0.30f))
                            drawRect(Color.White.copy(alpha = 0.10f))
                        }
                    )
            )
            Row(Modifier.fillMaxSize()) {
                items.forEachIndexed { i, title ->
                    Box(
                        Modifier
                            .weight(1f)
                            .fillMaxHeight()
                            .clickable { onSelect(i) },
                        contentAlignment = Alignment.Center
                    ) {
                        BasicText(
                            title,
                            style = TextStyle(
                                if (i == index) GlassText
                                else GlassText.copy(alpha = 0.62f),
                                13f.sp,
                                fontWeight = if (i == index) FontWeight.SemiBold
                                else FontWeight.Normal
                            )
                        )
                    }
                }
            }
        }
    }
}

/** The little magnifier glyph used by the search field. */
@Composable
fun MagnifierGlyph(
    modifier: Modifier = Modifier,
    color: Color = GlassText.copy(alpha = 0.85f)
) {
    Canvas(modifier) {
        val w = size.minDimension
        val radius = w * 0.30f
        val center = androidx.compose.ui.geometry.Offset(w * 0.40f, w * 0.40f)
        val stroke = w * 0.10f
        drawCircle(
            color = color,
            radius = radius,
            center = center,
            style = Stroke(width = stroke)
        )
        drawLine(
            color = color,
            start = androidx.compose.ui.geometry.Offset(
                center.x + radius * 0.72f,
                center.y + radius * 0.72f
            ),
            end = androidx.compose.ui.geometry.Offset(w * 0.92f, w * 0.92f),
            strokeWidth = stroke,
            cap = StrokeCap.Round
        )
    }
}

/** A liquid search field: glass capsule, magnifier glyph, blinking text. */
@Composable
fun LiquidSearchField(
    value: String,
    onValueChange: (String) -> Unit,
    placeholder: String,
    backdrop: Backdrop,
    modifier: Modifier = Modifier
) {
    LiquidSurface(
        backdrop = backdrop,
        modifier = modifier,
        radius = 24f.dp,
        blurRadius = 8f.dp,
        lensHeight = 8f.dp,
        lensAmount = 16f.dp,
        surfaceAlpha = 0.12f
    ) {
        Row(
            Modifier
                .fillMaxSize()
                .padding(horizontal = 14f.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            MagnifierGlyph(Modifier.size(16f.dp))
            Spacer(Modifier.width(10f.dp))
            Box(Modifier.weight(1f)) {
                if (value.isEmpty()) {
                    BasicText(
                        placeholder,
                        style = TextStyle(GlassText.copy(alpha = 0.45f), 14f.sp)
                    )
                }
                BasicTextField(
                    value = value,
                    onValueChange = onValueChange,
                    modifier = Modifier.fillMaxWidth(),
                    textStyle = TextStyle(GlassText, 14f.sp),
                    cursorBrush = SolidColor(GlassWhite),
                    singleLine = true
                )
            }
        }
    }
}

/** An iOS style segmented control: glass pill with a sliding lens. */
@Composable
fun LiquidSegmented(
    items: List<String>,
    selectedIndex: Int,
    onSelect: (Int) -> Unit,
    backdrop: Backdrop,
    modifier: Modifier = Modifier
) {
    LiquidSurface(
        backdrop = backdrop,
        modifier = modifier,
        radius = 26f.dp,
        blurRadius = 6f.dp,
        lensHeight = 6f.dp,
        lensAmount = 12f.dp,
        surfaceAlpha = 0.08f
    ) {
        if (items.isNotEmpty()) {
            val index = selectedIndex.coerceIn(0, items.size - 1)
            BoxWithConstraints(Modifier.fillMaxSize()) {
                val segmentWidth = maxWidth / items.size
                val fraction = index.toFloat()
                Box(
                    Modifier
                        .graphicsLayer {
                            translationX = segmentWidth.toPx() * fraction
                        }
                        .width(segmentWidth)
                        .fillMaxHeight()
                        .padding(3f.dp)
                        .drawBackdrop(
                            backdrop = backdrop,
                            shape = { Capsule() },
                            effects = {
                                vibrancy()
                                blur(2f.dp.toPx())
                                lens(10f.dp.toPx(), 18f.dp.toPx(), chromaticAberration = true)
                            },
                            highlight = { Highlight.Ambient },
                            shadow = {
                                Shadow(
                                    radius = 10f.dp,
                                    offset = DpOffset(0f.dp, 3f.dp),
                                    color = Color.Black.copy(alpha = 0.18f)
                                )
                            },
                            onDrawSurface = {
                                drawRect(Color.White.copy(alpha = 0.22f))
                            }
                        )
                )
            }
        }
        Row(Modifier.fillMaxSize()) {
            items.forEachIndexed { index, title ->
                Box(
                    Modifier
                        .weight(1f)
                        .fillMaxHeight()
                        .clickable { onSelect(index) },
                    contentAlignment = Alignment.Center
                ) {
                    BasicText(
                        title,
                        style = TextStyle(
                            if (index == selectedIndex) GlassText
                            else GlassText.copy(alpha = 0.6f),
                            13f.sp,
                            fontWeight = if (index == selectedIndex) FontWeight.SemiBold
                            else FontWeight.Normal
                        )
                    )
                }
            }
        }
    }
}

/** A glass card: headline plus body copy on a frosted panel. */
@Composable
fun LiquidCard(
    title: String,
    body: String,
    backdrop: Backdrop,
    modifier: Modifier = Modifier,
    accent: Color = GlassWhite
) {
    LiquidSurface(
        backdrop = backdrop,
        modifier = modifier,
        radius = 24f.dp,
        blurRadius = 10f.dp,
        lensHeight = 12f.dp,
        lensAmount = 24f.dp,
        surfaceAlpha = 0.10f
    ) {
        Column(
            Modifier
                .fillMaxSize()
                .padding(16f.dp)
        ) {
            Box(
                Modifier
                    .width(28f.dp)
                    .height(3f.dp)
                    .clip(Capsule())
                    .background(accent.copy(alpha = 0.9f))
            )
            Spacer(Modifier.height(10f.dp))
            BasicText(
                title,
                style = TextStyle(GlassText, 16f.sp, fontWeight = FontWeight.SemiBold)
            )
            Spacer(Modifier.height(6f.dp))
            BasicText(
                body,
                style = TextStyle(GlassText.copy(alpha = 0.72f), 13f.sp)
            )
        }
    }
}

/** A glass stepper: minus / value / plus, all on one capsule. */
@Composable
fun LiquidStepper(
    value: Int,
    onChange: (Int) -> Unit,
    backdrop: Backdrop,
    modifier: Modifier = Modifier
) {
    LiquidSurface(
        backdrop = backdrop,
        modifier = modifier,
        radius = 22f.dp,
        blurRadius = 6f.dp,
        lensHeight = 8f.dp,
        lensAmount = 16f.dp,
        surfaceAlpha = 0.10f
    ) {
        Row(
            Modifier
                .fillMaxSize()
                .padding(4f.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            StepperButton(backdrop, "−") { onChange(value - 1) }
            BasicText(
                value.toString(),
                modifier = Modifier.weight(1f),
                style = TextStyle(GlassText, 15f.sp, fontWeight = FontWeight.Medium)
            )
            StepperButton(backdrop, "+") { onChange(value + 1) }
        }
    }
}

@Composable
private fun StepperButton(backdrop: Backdrop, label: String, onClick: () -> Unit) {
    Box(
        Modifier
            .size(30f.dp)
            .clip(CircleShape)
            .clickable { onClick() }
            .drawBackdrop(
                backdrop = backdrop,
                shape = { CircleShape },
                effects = {
                    vibrancy()
                    blur(2f.dp.toPx())
                    lens(6f.dp.toPx(), 12f.dp.toPx(), chromaticAberration = true)
                },
                highlight = { Highlight.Ambient },
                onDrawSurface = { drawRect(Color.White.copy(alpha = 0.18f)) }
            ),
        contentAlignment = Alignment.Center
    ) {
        BasicText(label, style = TextStyle(GlassText, 16f.sp))
    }
}

/**
 * A draggable loupe that magnifies the layer behind it (the wallpaper).
 * The magnified content is the same backdrop layer, sampled through a
 * scaled graphics layer, so the lens really zooms in.
 */
@Composable
fun LiquidMagnifier(
    backdrop: Backdrop,
    modifier: Modifier = Modifier,
    zoom: Float = 2.8f,
    onDrag: (Float, Float) -> Unit = { _, _ -> }
) {
    LiquidDisc(
        backdrop = backdrop,
        modifier = modifier
            .draggable2D(
                state = rememberDraggable2DState { delta ->
                    onDrag(delta.x, delta.y)
                }
            ),
        zoom = zoom
    ) {}
}
