/*
    Copyright (C) 2000-2006 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <algorithm>

#include "pbd/pthread_utils.h"
#include "pbd/basename.h"
#include "pbd/shortpath.h"
#include "pbd/stateful_diff_command.h"

#include "ardour/audio_track.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/midi_model.h"
#include "ardour/operations.h"
#include "ardour/region_factory.h"
#include "ardour/smf_source.h"
#include "ardour/source_factory.h"
#include "ardour/utils.h"
#include "ardour/playlist.h"
#include "ardour/session.h"
#include "pbd/memento_command.h"

#include "ptformat/ptfformat.h"

#include "ardour_ui.h"
#include "cursor_context.h"
#include "editor.h"
#include "sfdb_ui.h"
#include "editing.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"
#include "session_import_dialog.h"
#include "gui_thread.h"
#include "interthread_progress_window.h"
#include "mouse_cursors.h"
#include "editor_cursors.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
using std::string;

/* Editor dialogs supporting the incorporation of PT sessions into ardour */

void
Editor::external_pt_dialog ()
{
	std::string ptpath;

	if (_session == 0) {
		MessageDialog msg (_("You can't import a PT session until you have a session loaded."));
		msg.run ();
		return;
	}

	Gtk::FileChooserDialog dialog (_("Import PT Session"), FILE_CHOOSER_ACTION_OPEN);
	dialog.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button (Gtk::Stock::OK, Gtk::RESPONSE_OK);

	while (true) {
		int result = dialog.run ();

		if (result == Gtk::RESPONSE_OK) {
			ptpath = dialog.get_filename ();

			if (!Glib::file_test (ptpath, Glib::FILE_TEST_IS_DIR|Glib::FILE_TEST_EXISTS)) {
				Gtk::MessageDialog msg (string_compose (_("%1: this is only the directory/folder name, not the filename.\n"), ptpath));
				msg.run ();
				continue;
			}
		}

		if (ptpath.length ()) {
			uint32_t srate = _session->sample_rate ();

			if (import_ptf.load (ptpath, srate) == -1) {
				MessageDialog msg (_("Doesn't seem to be a valid PT session file"));
				msg.run ();
				return;
			} else {
				MessageDialog msg (string_compose (_("PT v%1 Session @ %2Hz\n\n%3 audio files\n%4 audio regions\n%5 active audio regions\n%6 midi regions\n%7 active midi regions\n\nContinue..."), (int)import_ptf.version, import_ptf.sessionrate, import_ptf.audiofiles.size (), import_ptf.regions.size (), import_ptf.tracks.size (), import_ptf.midiregions.size (), import_ptf.miditracks.size ()));
				msg.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

				int result = msg.run ();
				if (result != Gtk::RESPONSE_OK) {
					return;
				}
			}

			import_pt_status.all_done = false;

			ImportProgressWindow ipw (&import_pt_status, _("PT Import"), _("Cancel Import"));
			pthread_create_and_store ("import_pt", &import_pt_status.thread, _import_pt_thread, this);
			pthread_detach (import_pt_status.thread);

			ipw.show();

			while (!import_pt_status.all_done) {
				gtk_main_iteration ();
			}

			// wait for thread to terminate
			while (!import_pt_status.done) {
				gtk_main_iteration ();
			}

			if (import_pt_status.cancel) {
				MessageDialog msg (_("PT import may have missing files, check session log for details"));
				msg.run ();
			} else {
				MessageDialog msg (_("PT import complete!"));
				msg.run ();
			}
			break;
		}

		if (result == Gtk::RESPONSE_CANCEL) {
			break;
		}
	}
}

void *
Editor::_import_pt_thread (void *arg)
{
	SessionEvent::create_per_thread_pool ("import pt events", 64);

	Editor *ed = (Editor *) arg;
	return ed->import_pt_thread ();
}

void *
Editor::import_pt_thread ()
{
	_session->import_pt (import_ptf, import_pt_status);
	return 0;
}
