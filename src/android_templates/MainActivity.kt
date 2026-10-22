package {{PACKAGE}}

import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent

/**
 * Hosts the transpiled Forge program.
 *
 * The program itself runs on a worker thread (Forge programs are written
 * as `while (gui.tick()) { ... }` loops); every widget operation is
 * marshalled onto the UI thread by ForgeGui.
 */
class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ForgeGui.attach()
        setContent {
            ForgeGui.Root()
        }

        val program = Thread({
            try {
                ForgeMain.main(arrayOf())
            } catch (t: Throwable) {
                Log.e("Forge", "program terminated", t)
            } finally {
                ForgeGui.shutdown()
            }
        }, "forge-main")
        program.isDaemon = true
        program.start()
    }

    override fun onDestroy() {
        ForgeGui.shutdown()
        super.onDestroy()
    }
}
