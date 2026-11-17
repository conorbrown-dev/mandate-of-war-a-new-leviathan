using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

internal static class Launcher
{
    [STAThread]
    private static void Main()
    {
        string root = AppDomain.CurrentDomain.BaseDirectory;
        string godot = Path.Combine(root, "GodotRuntime.exe");
        string project = Path.Combine(root, "godot", "project");
        if (!File.Exists(godot) || !File.Exists(Path.Combine(project, "project.godot")))
        {
            MessageBox.Show("The game files are incomplete. Extract the entire demo ZIP, then run MandateOfWar.exe again.",
                "Mandate of War", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        var start = new ProcessStartInfo
        {
            FileName = godot,
            Arguments = "--path \"" + project + "\"",
            WorkingDirectory = root,
            UseShellExecute = false
        };

        try
        {
            Process.Start(start);
        }
        catch (Exception exception)
        {
            MessageBox.Show("Could not start the game. " + exception.Message,
                "Mandate of War", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }
}
