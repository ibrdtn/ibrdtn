package ibrdtn.example.logging;

import ibrdtn.example.ui.DTNExampleApp;
import java.util.logging.Formatter;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.LogManager;
import java.util.logging.LogRecord;

/**
 * Redirects logging output to GUI.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class WindowHandler extends Handler {

    private DTNExampleApp gui = null;
    private Formatter formatter = null;
    private static WindowHandler handler = null;

    private WindowHandler(DTNExampleApp gui) {
        LogManager manager = LogManager.getLogManager();
        String className = this.getClass().getName();
        String level = manager.getProperty(className + ".level");
        setLevel(level != null ? Level.parse(level) : Level.INFO);
        if (this.gui == null) {
            this.gui = gui;
        }
    }

    public static synchronized WindowHandler getInstance(DTNExampleApp gui) {
        if (handler == null) {
            handler = new WindowHandler(gui);
        }
        return handler;
    }

    @Override
    public synchronized void publish(LogRecord record) {
        if (!isLoggable(record)) {
            return;
        }
        formatter = new LogFormatter();
        String message = formatter.format(record);
        gui.print(message);
    }

    @Override
    public void close() {
    }

    @Override
    public void flush() {
    }
}
