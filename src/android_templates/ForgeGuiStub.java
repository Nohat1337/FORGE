import java.util.List;
import java.util.Map;

/**
 * No-op implementation of the Forge `gui` module for plain JVM runs.
 *
 * The real implementation is the Android one (ForgeGui in the app), which
 * renders every widget with the liquid glass library. On a desktop JVM there
 * is no window system, so the module answers with inert placeholder ids.
 */
public final class ForgeGui {
    private static long next = 1L;
    private static final Map<String, Object> TEXT = new java.util.HashMap<>();

    private ForgeGui() {}

    private static Object id() { return next++; }

    public static Object window(List<Object> a) { return id(); }
    public static Object button(List<Object> a) { return id(); }
    public static Object label(List<Object> a) { return id(); }
    public static Object textfield(List<Object> a) { return id(); }
    public static Object checkbox(List<Object> a) { return id(); }
    public static Object progress(List<Object> a) { return id(); }
    public static Object panel(List<Object> a) { return id(); }
    public static Object slider(List<Object> a) { return id(); }
    public static Object gauge(List<Object> a) { return id(); }
    public static Object toggle(List<Object> a) { return id(); }
    public static Object radio(List<Object> a) { return id(); }
    public static Object tabs(List<Object> a) { return id(); }
    public static Object list(List<Object> a) { return id(); }
    public static Object bottombar(List<Object> a) { return id(); }
    public static Object combo(List<Object> a) { return id(); }
    public static Object search(List<Object> a) { return id(); }
    public static Object segmented(List<Object> a) { return id(); }
    public static Object card(List<Object> a) { return id(); }
    public static Object magnifier(List<Object> a) { return id(); }
    public static Object stepper(List<Object> a) { return id(); }

    public static Object set_text(List<Object> a) { return null; }
    public static Object get_text(List<Object> a) { return ""; }
    public static Object set_checked(List<Object> a) { return null; }
    public static Object set_progress(List<Object> a) { return null; }
    public static Object set_value(List<Object> a) { return null; }
    public static Object get_value(List<Object> a) { return 0L; }
    public static Object set_items(List<Object> a) { return null; }
    public static Object select(List<Object> a) { return null; }
    public static Object get_selected(List<Object> a) { return -1L; }
    public static Object set_glass(List<Object> a) { return null; }
    public static Object set_tint(List<Object> a) { return null; }
    public static Object close(List<Object> a) { return null; }

    public static Object tick(List<Object> a) { return Boolean.FALSE; }
    public static Object poll(List<Object> a) { return null; }
    public static Object screenshot(List<Object> a) { return Boolean.FALSE; }
}
