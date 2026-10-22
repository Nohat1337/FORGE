package {{PACKAGE}}

import android.graphics.Color as AndroidColor
import android.os.Handler
import android.os.Looper
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.kyant.backdrop.Backdrop
import com.kyant.backdrop.backdrops.layerBackdrop
import com.kyant.backdrop.backdrops.rememberLayerBackdrop
import com.kyant.backdrop.drawBackdrop
import com.kyant.backdrop.effects.blur
import com.kyant.backdrop.effects.lens
import com.kyant.backdrop.effects.vibrancy
import com.kyant.shapes.Capsule
import java.util.concurrent.CountDownLatch
import java.util.concurrent.LinkedBlockingQueue
import kotlin.math.roundToInt

/**
 * The Forge `gui` module, rendered with Jetpack Compose and the Kyant0
 * liquid glass library (`io.github.kyant0:backdrop`).
 *
 * The transpiled Forge program calls the static functions below exactly
 * like the desktop runtime does; every widget is a Compose node placed at
 * absolute coordinates, and the liquid components (button / toggle /
 * slider / bottom tabs) come from the library's own catalog.
 */
object ForgeGui {

    private val UI = Handler(Looper.getMainLooper())
    private val widgets = mutableStateListOf<ForgeWidget>()
    private val events = LinkedBlockingQueue<Any>()
    private var nextId = 1L

    @Volatile
    var alive: Boolean = true

    fun attach() {
        alive = true
    }

    fun shutdown() {
        alive = false
        events.clear()
    }

    // ---------------------------------------------------------------
    // widget state
    // ---------------------------------------------------------------

    class ForgeWidget(val id: Long, val kind: String) {
        var x by mutableIntStateOf(0)
        var y by mutableIntStateOf(0)
        var w by mutableIntStateOf(120)
        var h by mutableIntStateOf(40)
        var text by mutableStateOf("")
        var value by mutableFloatStateOf(0f)
        var checked by mutableStateOf(false)
        var selected by mutableIntStateOf(-1)
        var items by mutableStateOf(emptyList<String>())
        var group by mutableStateOf("")
    }

    // ---------------------------------------------------------------
    // thread helpers
    // ---------------------------------------------------------------

    private fun onUi(block: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) block() else UI.post(block)
    }

    private fun <T> onUiSync(block: () -> T): T {
        if (Looper.myLooper() == Looper.getMainLooper()) return block()
        val latch = CountDownLatch(1)
        var result: Any? = null
        UI.post {
            try {
                result = block()
            } finally {
                latch.countDown()
            }
        }
        latch.await(500, java.util.concurrent.TimeUnit.MILLISECONDS)
        @Suppress("UNCHECKED_CAST")
        return result as T
    }

    fun push(type: String, widgetId: Long, text: String) {
        events.add(arrayOf<Any>(type, widgetId, text))
    }

    // ---------------------------------------------------------------
    // argument helpers
    // ---------------------------------------------------------------

    private fun num(a: List<*>, i: Int): Double = (a.getOrNull(i) as? Number)?.toDouble() ?: 0.0
    private fun lng(a: List<*>, i: Int): Long = (a.getOrNull(i) as? Number)?.toLong() ?: 0L
    private fun int(a: List<*>, i: Int): Int = (a.getOrNull(i) as? Number)?.toInt() ?: 0
    private fun str(a: List<*>, i: Int): String = a.getOrNull(i)?.toString() ?: ""
    private fun bool(a: List<*>, i: Int): Boolean = (a.getOrNull(i) as? Boolean) ?: false
    private fun split(s: String): List<String> =
        if (s.isEmpty()) emptyList() else s.split("|")

    private fun add(kind: String, a: List<*>, xIdx: Int, yIdx: Int, wIdx: Int, hIdx: Int): ForgeWidget {
        val w = ForgeWidget(nextId++, kind)
        w.x = int(a, xIdx)
        w.y = int(a, yIdx)
        w.w = if (a.size > wIdx) int(a, wIdx) else 120
        w.h = if (a.size > hIdx) int(a, hIdx) else 40
        onUi { widgets.add(w) }
        return w
    }

    // ---------------------------------------------------------------
    // module entry points (called from the transpiled program)
    // ---------------------------------------------------------------

    @JvmStatic
    fun window(a: List<*>): Any? {
        // the whole screen is the window
        return 0L
    }

    @JvmStatic
    fun button(a: List<*>): Any? {
        val w = add("button", a, 1, 2, 3, 4)
        w.text = str(a, 5)
        return w.id
    }

    @JvmStatic
    fun label(a: List<*>): Any? {
        val w = add("label", a, 1, 2, 3, 4)
        w.text = str(a, if (a.size >= 6) 5 else 3)
        return w.id
    }

    @JvmStatic
    fun textfield(a: List<*>): Any? {
        val w = add("textfield", a, 1, 2, 3, 4)
        w.text = str(a, 5)
        return w.id
    }

    @JvmStatic
    fun checkbox(a: List<*>): Any? {
        val w = add("checkbox", a, 1, 2, 3, 4)
        w.text = str(a, if (a.size >= 5) 3 else 3)
        w.checked = bool(a, if (a.size >= 5) 4 else 3)
        return w.id
    }

    @JvmStatic
    fun progress(a: List<*>): Any? {
        val w = add("progress", a, 1, 2, 3, 4)
        w.value = num(a, 5).toFloat()
        return w.id
    }

    @JvmStatic
    fun panel(a: List<*>): Any? {
        val w = add("panel", a, 1, 2, 3, 4)
        return w.id
    }

    @JvmStatic
    fun slider(a: List<*>): Any? {
        val w = add("slider", a, 1, 2, 3, 4)
        w.value = num(a, 5).toFloat()
        return w.id
    }

    @JvmStatic
    fun gauge(a: List<*>): Any? {
        val w = add("gauge", a, 1, 2, 3, 4)
        w.value = num(a, 4).toFloat()
        w.text = str(a, 5)
        return w.id
    }

    @JvmStatic
    fun toggle(a: List<*>): Any? {
        val w = add("toggle", a, 1, 2, 3, 4)
        w.text = str(a, 3)
        w.checked = bool(a, 4)
        return w.id
    }

    @JvmStatic
    fun radio(a: List<*>): Any? {
        val w = add("radio", a, 1, 2, 3, 4)
        w.text = str(a, 3)
        w.group = str(a, 4)
        w.checked = bool(a, 5)
        return w.id
    }

    @JvmStatic
    fun tabs(a: List<*>): Any? {
        val w = add("tabs", a, 1, 2, 3, 4)
        w.items = split(str(a, 5))
        w.selected = 0
        return w.id
    }

    @JvmStatic
    fun list(a: List<*>): Any? {
        val w = add("list", a, 1, 2, 3, 4)
        w.items = split(str(a, 5))
        return w.id
    }

    @JvmStatic
    fun combo(a: List<*>): Any? {
        val w = add("combo", a, 1, 2, 3, 4)
        w.items = split(str(a, 5))
        w.selected = 0
        return w.id
    }

    // ---------------------------------------------------------------
    // property access
    // ---------------------------------------------------------------

    private fun widget(a: List<*>): ForgeWidget? =
        widgets.firstOrNull { it.id == lng(a, 0) }

    @JvmStatic
    fun set_text(a: List<*>): Any? {
        val t = str(a, 1)
        onUi { widget(a)?.text = t }
        return null
    }

    @JvmStatic
    fun get_text(a: List<*>): Any? = widget(a)?.text ?: ""

    @JvmStatic
    fun set_checked(a: List<*>): Any? {
        val v = bool(a, 1)
        onUi { widget(a)?.checked = v }
        return null
    }

    @JvmStatic
    fun set_progress(a: List<*>): Any? {
        val v = num(a, 1).toFloat()
        onUi { widget(a)?.value = v }
        return null
    }

    @JvmStatic
    fun set_value(a: List<*>): Any? {
        val v = num(a, 1).toFloat()
        onUi { widget(a)?.value = v }
        return null
    }

    @JvmStatic
    fun get_value(a: List<*>): Any? {
        val w = widget(a) ?: return 0L
        return if (w.kind == "slider") w.value.toDouble() else w.value.toLong()
    }

    @JvmStatic
    fun set_items(a: List<*>): Any? {
        val items = split(str(a, 1))
        onUi { widget(a)?.items = items }
        return null
    }

    @JvmStatic
    fun select(a: List<*>): Any? {
        val i = int(a, 1)
        onUi { widget(a)?.selected = i }
        return null
    }

    @JvmStatic
    fun get_selected(a: List<*>): Any? = widget(a)?.selected ?: -1L

    @JvmStatic
    fun close(a: List<*>): Any? {
        val id = lng(a, 0)
        onUi { widgets.removeAll { it.id == id } }
        return null
    }

    @JvmStatic
    fun tick(a: List<*>): Any? = alive

    @JvmStatic
    fun poll(a: List<*>): Any? {
        val ev = events.poll(50, java.util.concurrent.TimeUnit.MILLISECONDS) ?: return null
        val arr = ev as Array<*>
        val m = LinkedHashMap<String, Any>()
        m["window"] = 0L
        m["id"] = arr[1] as Long
        m["type"] = arr[0] as String
        m["text"] = arr[2] as String
        return m
    }

    @JvmStatic
    fun screenshot(a: List<*>): Any? = false

    // ---------------------------------------------------------------
    // Compose rendering
    // ---------------------------------------------------------------

    @Composable
    fun Root() {
        val backdrop = rememberLayerBackdrop()
        Box(Modifier.fillMaxSize()) {
            ForgeWallpaper(Modifier.layerBackdrop(backdrop).fillMaxSize())
            widgets.forEach { w -> WidgetNode(w, backdrop) }
        }
    }

    @Composable
    private fun WidgetNode(w: ForgeWidget, backdrop: Backdrop) {
        Box(
            Modifier
                .offset { IntOffset(w.x.dp.roundToPx(), w.y.dp.roundToPx()) }
                .size(w.w.dp, w.h.dp)
        ) {
            when (w.kind) {
                "button" -> LiquidButton(
                    onClick = { push("click", w.id, "") },
                    backdrop = backdrop,
                    modifier = Modifier.fillMaxSize(),
                    surfaceColor = Color.White.copy(alpha = 0.10f)
                ) {
                    BasicText(
                        w.text,
                        modifier = Modifier.padding(horizontal = 12f.dp),
                        style = TextStyle(Color.White, 15f.sp)
                    )
                }

                "toggle" -> Row(
                    Modifier.fillMaxSize(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(10f.dp)
                ) {
                    LiquidToggle(
                        selected = { w.checked },
                        onSelect = { v ->
                            w.checked = v
                            push("check", w.id, if (v) "1" else "0")
                        },
                        backdrop = backdrop
                    )
                    BasicText(w.text, style = TextStyle(Color.White, 14f.sp))
                }

                "slider" -> Column(
                    Modifier.fillMaxSize(),
                    verticalArrangement = Arrangement.Center
                ) {
                    LiquidSlider(
                        value = { w.value },
                        onValueChange = { v ->
                            w.value = v
                            push("change", w.id, v.toString())
                        },
                        valueRange = 0f..1f,
                        visibilityThreshold = 0.001f,
                        backdrop = backdrop,
                        modifier = Modifier.fillMaxWidth()
                    )
                }

                "tabs" -> LiquidBottomTabs(
                    selectedTabIndex = { w.selected },
                    onTabSelected = { i ->
                        w.selected = i
                        push("select", w.id, i.toString())
                    },
                    backdrop = backdrop,
                    tabsCount = w.items.size.coerceAtLeast(1),
                    modifier = Modifier.fillMaxSize()
                ) {
                    w.items.forEachIndexed { index, title ->
                        LiquidBottomTab(onClick = {
                            w.selected = index
                            push("select", w.id, index.toString())
                        }) {
                            BasicText(title, style = TextStyle(Color.White, 12f.sp))
                        }
                    }
                }

                "checkbox" -> Row(
                    Modifier.fillMaxSize(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8f.dp)
                ) {
                    GlassBox(backdrop, Modifier.size(22f.dp), 11f.dp) {
                        if (w.checked) {
                            Box(
                                Modifier
                                    .padding(5f.dp)
                                    .fillMaxSize()
                                    .clip(CircleShape)
                                    .background(Color(0xFF0A84FF))
                            )
                        }
                    }
                    BasicText(w.text, style = TextStyle(Color.White, 14f.sp))
                }

                "radio" -> Row(
                    Modifier.fillMaxSize(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8f.dp)
                ) {
                    GlassBox(backdrop, Modifier.size(22f.dp), 11f.dp) {
                        if (w.checked) {
                            Box(
                                Modifier
                                    .padding(6f.dp)
                                    .fillMaxSize()
                                    .clip(CircleShape)
                                    .background(Color(0xFF0A84FF))
                            )
                        }
                    }
                    BasicText(w.text, style = TextStyle(Color.White, 14f.sp))
                }

                "label" -> BasicText(
                    w.text,
                    modifier = Modifier.fillMaxSize(),
                    style = TextStyle(Color.White, 14f.sp)
                )

                "textfield" -> GlassBox(backdrop, Modifier.fillMaxSize(), 14f.dp) {
                    BasicTextField(
                        value = w.text,
                        onValueChange = {
                            w.text = it
                            push("text", w.id, it)
                        },
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(horizontal = 12f.dp, vertical = 8f.dp),
                        textStyle = TextStyle(Color.White, 14f.sp),
                        cursorBrush = androidx.compose.ui.graphics.SolidColor(Color(0xFF0A84FF))
                    )
                }

                "progress" -> GlassBox(backdrop, Modifier.fillMaxSize(), 14f.dp) {
                    val fraction by animateFloatAsState(
                        targetValue = w.value.coerceIn(0f, 1f),
                        label = "progress"
                    )
                    Box(
                        Modifier
                            .fillMaxWidth()
                            .padding(3f.dp)
                            .height((w.h - 6).dp.coerceAtLeast(4f.dp))
                            .clip(Capsule())
                            .background(Color.White.copy(alpha = 0.18f))
                    ) {
                        Box(
                            Modifier
                                .fillMaxWidth(fraction)
                                .fillMaxHeight()
                                .clip(Capsule())
                                .background(Color(0xFF0A84FF).copy(alpha = 0.85f))
                        )
                    }
                    BasicText(
                        "${(fraction * 100).roundToInt()}%",
                        modifier = Modifier.align(Alignment.Center),
                        style = TextStyle(Color.White, 12f.sp)
                    )
                }

                "gauge" -> GlassBox(backdrop, Modifier.fillMaxSize(), 16f.dp) {
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        androidx.compose.foundation.Canvas(Modifier.fillMaxSize()) {
                            val stroke = Stroke(width = 10f.dp.toPx())
                            val diameter = size.minDimension - stroke.width
                            val topLeft = androidx.compose.ui.geometry.Offset(
                                (size.width - diameter) / 2f,
                                (size.height - diameter) / 2f
                            )
                            drawArc(
                                color = Color.White.copy(alpha = 0.15f),
                                startAngle = 135f,
                                sweepAngle = 270f,
                                useCenter = false,
                                topLeft = topLeft,
                                size = androidx.compose.ui.geometry.Size(diameter, diameter),
                                style = stroke
                            )
                            drawArc(
                                color = Color(0xFF30D158),
                                startAngle = 135f,
                                sweepAngle = 270f * w.value.coerceIn(0f, 1f),
                                useCenter = false,
                                topLeft = topLeft,
                                size = androidx.compose.ui.geometry.Size(diameter, diameter),
                                style = stroke
                            )
                        }
                        BasicText(
                            w.text.ifEmpty { "${(w.value * 100).roundToInt()}%" },
                            style = TextStyle(Color.White, 16f.sp)
                        )
                    }
                }

                "list" -> GlassBox(backdrop, Modifier.fillMaxSize(), 16f.dp) {
                    Column(
                        Modifier
                            .fillMaxSize()
                            .padding(vertical = 6f.dp),
                        verticalArrangement = Arrangement.spacedBy(2f.dp)
                    ) {
                        w.items.forEachIndexed { index, item ->
                            Row(
                                Modifier
                                    .fillMaxWidth()
                                    .height(32f.dp)
                                    .clickable { push("select", w.id, index.toString()) }
                                    .padding(horizontal = 14f.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                BasicText(item, style = TextStyle(Color.White, 14f.sp))
                            }
                        }
                    }
                }

                "combo" -> GlassBox(backdrop, Modifier.fillMaxSize(), 14f.dp) {
                    Row(
                        Modifier
                            .fillMaxSize()
                            .clickable {
                                if (w.items.isNotEmpty()) {
                                    val next = (w.selected + 1) % w.items.size
                                    w.selected = next
                                    push("select", w.id, next.toString())
                                }
                            }
                            .padding(horizontal = 14f.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        BasicText(
                            w.items.getOrNull(w.selected) ?: "",
                            modifier = Modifier.weight(1f),
                            style = TextStyle(Color.White, 14f.sp)
                        )
                        BasicText("▾", style = TextStyle(Color.White, 12f.sp))
                    }
                }

                "panel" -> GlassBox(backdrop, Modifier.fillMaxSize(), 20f.dp) {}

                else -> BasicText(w.text, style = TextStyle(Color.White, 14f.sp))
            }
        }
    }

    /** A rounded glass surface drawn with the backdrop library. */
    @Composable
    private fun GlassBox(
        backdrop: Backdrop,
        modifier: Modifier,
        radius: androidx.compose.ui.unit.Dp,
        content: @Composable () -> Unit
    ) {
        Box(
            modifier
                .drawBackdrop(
                    backdrop = backdrop,
                    shape = { RoundedCornerShape(radius) },
                    effects = {
                        vibrancy()
                        blur(6f.dp.toPx())
                        lens(10f.dp.toPx(), 20f.dp.toPx())
                    },
                    onDrawSurface = {
                        drawRect(Color.White.copy(alpha = 0.10f))
                    }
                ),
            content = { content() }
        )
    }
}
