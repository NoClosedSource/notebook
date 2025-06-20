#include <gtk/gtk.h>
#include <gdk/gdk.h>

int current_font_size = 12;
int min_font_size = 1;
int max_font_size = 144;

int max_undo_history = 100;

typedef struct {
    gchar *text;
} UndoState;

typedef struct {
    GtkAdjustment *vadj;
    GtkAdjustment *hadj;
    gdouble vscroll;
    gdouble hscroll;
} ScrollState;

GtkWidget *window;
GtkWidget *text_view;
GtkWidget *info_text;
GtkTextTag *font_tag;

// find
// ctrl + f
GtkWidget *search_entry;
GtkWidget *search_bar;
GtkWidget *match_label;
GtkWidget *case_checkbox;
GtkWidget *find_next_button;
GtkWidget *find_prev_button;

// replace
// ctrl + g
GtkWidget *replace_bar;
GtkWidget *replace_match_label;
GtkWidget *replace_find_entry; // replaced
GtkWidget *replace_with_entry; // replacer
GtkWidget *replace_one_button;
GtkWidget *replace_all_button;
GtkWidget *replace_find_next_button;
GtkWidget *replace_find_prev_button;
GtkWidget *replace_case_checkbox;

// undo & redo
// ctrl + z, ctrl + y
GList *undo_stack = NULL;
GList *redo_stack = NULL;
gboolean undoing = FALSE;
gboolean redoing = FALSE;

// highlighting
GtkTextTag *highlight_tag;
GArray *search_matches = NULL;
int current_match_index = -1;

// helper function
int clamp(int x, int min, int max) {
    if (x < min) 
        return min;

    if (x > max) 
        return max;

    return x;
}

void new_file(GtkWidget *widget, gpointer data) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, "", -1);
}

void open_file(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    dialog = gtk_file_chooser_dialog_new(
        "Open File",
        GTK_WINDOW(data),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL
    );

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        char *contents;
        gsize length;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (g_file_get_contents(filename, &contents, &length, NULL)) {
            gtk_text_buffer_set_text(buffer, contents, length);
            g_free(contents);
        }

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

void save_file(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    dialog = gtk_file_chooser_dialog_new(
        "Save File", 
        GTK_WINDOW(data), 
        GTK_FILE_CHOOSER_ACTION_SAVE, 
        "_Cancel", 
        GTK_RESPONSE_CANCEL, 
        "_Save", 
        GTK_RESPONSE_ACCEPT, 
        NULL
    );

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        char *text;
        GtkTextIter start, end;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_text_buffer_get_start_iter(buffer, &start);
        gtk_text_buffer_get_end_iter(buffer, &end);
        text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

        g_file_set_contents(filename, text, -1, NULL);

        g_free(filename);
        g_free(text);
    }

    gtk_widget_destroy(dialog);
}

void quit_app(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
}

void apply_font_tag_to_buffer() {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter start, end;

    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_apply_tag(buffer, font_tag, &start, &end);
}

void update_label_text() {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);

    gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    
    int char_count = g_utf8_strlen(text, -1);
    int line_count = gtk_text_iter_get_line(&end) + 1;

    gchar *info = g_strdup_printf(" Characters: %d  Lines: %d  Text Size: %d", char_count, line_count, current_font_size);
    gtk_label_set_text(GTK_LABEL(info_text), info);

    g_free(text);
    g_free(info);
}

void update_match_label() {
    if (search_matches) {
        int total = search_matches->len / 2;
        gchar *label_text = g_strdup_printf("%d match%s ", total, total == 1 ? "" : "es");
        gtk_label_set_text(GTK_LABEL(match_label), label_text);

        gchar *replace_label_text = g_strdup_printf(" %d match%s ", total, total == 1 ? "" : "es");
        gtk_label_set_text(GTK_LABEL(replace_match_label), label_text);

        g_free(replace_label_text);
        g_free(label_text);
    } else {
        gtk_label_set_text(GTK_LABEL(match_label), "0 matches ");
        gtk_label_set_text(GTK_LABEL(match_label), " 0 matches ");
    }
}

void update_font_size() {
    gchar *font_str = g_strdup_printf("Monospace %d", current_font_size);

    g_object_set(font_tag, "font", font_str, NULL);
    g_free(font_str);

    apply_font_tag_to_buffer();
    update_label_text();
}

void zoom_in(GtkWidget *widget, gpointer data) {
    if (current_font_size < max_font_size) {
        current_font_size += 1;
        update_font_size();
    }
}

void zoom_out(GtkWidget *widget, gpointer data) {
    if (current_font_size > min_font_size) {
        current_font_size -= 1;
        update_font_size();
    }
}

gboolean restore_scroll_position(gpointer data) {
    ScrollState *scroll = (ScrollState *)data;
    gtk_adjustment_set_value(scroll->vadj, scroll->vscroll);
    gtk_adjustment_set_value(scroll->hadj, scroll->hscroll);
    g_free(scroll);
    return G_SOURCE_REMOVE;
}

void free_undo_stack(GList *stack) {
    g_list_free_full(stack, (GDestroyNotify)g_free);
}

void push_undo_state() {
    if (undoing || redoing) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);

    UndoState *state = g_new(UndoState, 1);
    state->text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    undo_stack = g_list_prepend(undo_stack, state);
    g_list_free_full(redo_stack, (GDestroyNotify)g_free);
    redo_stack = NULL;

    if (g_list_length(undo_stack) > max_undo_history) {
        GList *last_allowed = g_list_nth(undo_stack, max_undo_history - 1);
        GList *excess = last_allowed->next;
        last_allowed->next = NULL;

        g_list_free_full(excess, (GDestroyNotify)g_free);
    }
}

void restore_undo_state(UndoState *state) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    GtkWidget *scrolled_window = gtk_widget_get_parent(text_view);

    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
    GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
    gdouble vscroll = gtk_adjustment_get_value(vadj);
    gdouble hscroll = gtk_adjustment_get_value(hadj);

    // Set text
    gtk_text_buffer_set_text(buffer, state->text, -1);

    // Save scroll info to struct
    ScrollState *scroll = g_new(ScrollState, 1);
    scroll->vadj = vadj;
    scroll->hadj = hadj;
    scroll->vscroll = vscroll;
    scroll->hscroll = hscroll;

    // Restore scroll after buffer layout
    g_idle_add(restore_scroll_position, scroll);
}

void undo() {
    if (!undo_stack || !undo_stack->next) return;
    undoing = TRUE;

    UndoState *current = (UndoState *)undo_stack->data;
    redo_stack = g_list_prepend(redo_stack, current);

    undo_stack = g_list_delete_link(undo_stack, undo_stack);
    UndoState *prev = (UndoState *)undo_stack->data;
    restore_undo_state(prev);

    undoing = FALSE;
}

void redo() {
    if (!redo_stack) return;
    redoing = TRUE;

    UndoState *state = (UndoState *)redo_stack->data;
    undo_stack = g_list_prepend(undo_stack, state);

    redo_stack = g_list_delete_link(redo_stack, redo_stack);
    restore_undo_state(state);

    redoing = FALSE;
}

void on_text_changed(GtkTextBuffer *buffer, gpointer user_data) {
    if (!undoing && !redoing) {
        g_list_free_full(redo_stack, (GDestroyNotify)g_free);
        redo_stack = NULL;

        push_undo_state();
    }

    update_font_size();
}

void clear_search_highlights() {
    if (!search_matches) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    for (guint i = 0; i + 1 < search_matches->len; i += 2) {
        int start_offset = g_array_index(search_matches, int, i);
        int end_offset = g_array_index(search_matches, int, i + 1);
        GtkTextIter s_iter, e_iter;
        gtk_text_buffer_get_iter_at_offset(buffer, &s_iter, start_offset);
        gtk_text_buffer_get_iter_at_offset(buffer, &e_iter, end_offset);
        gtk_text_buffer_remove_tag(buffer, highlight_tag, &s_iter, &e_iter);
    }

    g_array_free(search_matches, TRUE);
    search_matches = NULL;
    current_match_index = -1;
}

void select_match(int index) {
    if (!search_matches || search_matches->len < 2 || index < 0)
        return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    int total = search_matches->len / 2;

    index = clamp(index, 0, total - 1);
    current_match_index = index;

    GtkTextIter *start = &g_array_index(search_matches, GtkTextIter, current_match_index * 2);
    GtkTextIter *end = &g_array_index(search_matches, GtkTextIter, current_match_index * 2 + 1);
    gtk_text_buffer_select_range(buffer, start, end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(text_view), start, 0.2, TRUE, 0.5, 0.0);

    update_match_label();
}

gboolean get_case_sensitive_setting() {
    return gtk_widget_is_visible(replace_bar)
        ? gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(replace_case_checkbox))
        : gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(case_checkbox));
}

void on_set_undo_limit_activate(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Set Max Undo History",
        GTK_WINDOW(window),
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_OK", GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *spin = gtk_spin_button_new_with_range(1, 1000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), max_undo_history);
    gtk_container_add(GTK_CONTAINER(content_area), spin);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        max_undo_history = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
        
        if (g_list_length(undo_stack) > max_undo_history) {
            GList *last_allowed = g_list_nth(undo_stack, max_undo_history - 1);
            GList *excess = last_allowed->next;
            last_allowed->next = NULL;
            g_list_free_full(excess, (GDestroyNotify)g_free);
        }
    }

    gtk_widget_destroy(dialog);
}

void on_search_activate(GtkEntry *entry, gpointer user_data) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter start, match_start, match_end;

    const gchar *search_text = NULL;
    if (gtk_widget_is_visible(replace_bar)) {
        search_text = gtk_entry_get_text(GTK_ENTRY(replace_find_entry));
    } else {
        search_text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    }

    gboolean case_sensitive = get_case_sensitive_setting();

    clear_search_highlights();
    if (!search_text || !*search_text) {
        gtk_label_set_text(GTK_LABEL(match_label), "0 matches ");
        gtk_label_set_text(GTK_LABEL(replace_match_label), " 0 matches ");
        return;
    }

    gtk_text_buffer_get_start_iter(buffer, &start);
    search_matches = g_array_new(FALSE, FALSE, sizeof(GtkTextIter));

    while (!gtk_text_iter_is_end(&start)) {
        GtkTextIter tmp = start;
        GtkTextIter end;
        gunichar *buffer_text = NULL;

        gtk_text_buffer_get_end_iter(buffer, &end);
        gchar *text = gtk_text_iter_get_text(&tmp, &end);
        gchar *pos = NULL;

        if (case_sensitive) {
            pos = g_strstr_len(text, -1, search_text);
        } else {
            gchar *lower_text = g_utf8_strdown(text, -1);
            gchar *lower_search = g_utf8_strdown(search_text, -1);
            gchar *match = g_strstr_len(lower_text, -1, lower_search);

            if (match) {
                pos = text + (match - lower_text);
            }

            g_free(lower_text);
            g_free(lower_search);
        }

        if (!pos) {
            g_free(text);
            break;
        }

        int offset = g_utf8_pointer_to_offset(text, pos);
        gtk_text_iter_set_offset(&start, gtk_text_iter_get_offset(&start) + offset);

        match_start = start;
        match_end = match_start;
        gtk_text_iter_forward_chars(&match_end, g_utf8_strlen(search_text, -1));

        g_array_append_val(search_matches, match_start);
        g_array_append_val(search_matches, match_end);

        gtk_text_buffer_apply_tag(buffer, highlight_tag, &match_start, &match_end);

        start = match_end;

        g_free(text);
    }

    update_match_label();

    if (search_matches->len > 0) {
        select_match(0);
    } else {
        gtk_label_set_text(GTK_LABEL(match_label), "0 matches ");
        gtk_label_set_text(GTK_LABEL(replace_match_label), " 0 matches ");
    }
}

void on_find_next_clicked(GtkButton *button, gpointer user_data) {
    if (!search_matches || search_matches->len == 0) return;
    int total = search_matches->len / 2;
    select_match((current_match_index + 1) % total);
}

void on_find_prev_clicked(GtkButton *button, gpointer user_data) {
    if (!search_matches || search_matches->len == 0) return;
    int total = search_matches->len / 2;
    select_match((current_match_index - 1 + total) % total);
}

void show_search_bar() {
    gtk_widget_hide(replace_bar);
    gtk_widget_show(search_bar);

    gtk_widget_grab_focus(search_entry);
    on_search_activate(NULL, NULL);
}

void show_replace_bar() {
    gtk_widget_hide(search_bar);
    gtk_widget_show(replace_bar);

    gtk_widget_grab_focus(replace_find_entry);
    on_search_activate(NULL, NULL);
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    switch (event->keyval) {
        case GDK_KEY_Escape:
            gtk_widget_hide(search_bar);
            gtk_widget_hide(replace_bar);
            clear_search_highlights();
            gtk_widget_grab_focus(text_view);
            return TRUE;
    }

    if (event->state & GDK_CONTROL_MASK) {
        switch (event->keyval) {
            case GDK_KEY_plus:
            case GDK_KEY_equal:
                if (current_font_size < max_font_size) {
                    current_font_size += 1;
                    update_font_size();
                }
                return TRUE;

            case GDK_KEY_minus:
                if (current_font_size > min_font_size) {
                    current_font_size -= 1;
                    update_font_size();
                }
                return TRUE;

            case GDK_KEY_0:
            case GDK_KEY_KP_0:
                current_font_size = 12;
                update_font_size();
                return TRUE;

            case GDK_KEY_z:
                undo();
                return TRUE;

            case GDK_KEY_y:
                redo();
                return TRUE;

            case GDK_KEY_f:
                show_search_bar();
                return TRUE;

            case GDK_KEY_g:
                show_replace_bar();
                return TRUE;

        }
    }

    if (gtk_widget_has_focus(search_entry)) {
        if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
            if (search_matches && search_matches->len > 0) {
                int total_matches = search_matches->len / 2;
            
                if (event->state & GDK_SHIFT_MASK) {
                    on_find_next_clicked(NULL, NULL);
                } else {
                    on_find_prev_clicked(NULL, NULL);
                }
            
                return TRUE;
            }
        }
    }

    return FALSE;
}

gboolean on_search_entry_text_changed() {
    clear_search_highlights();
    on_search_activate(NULL, NULL);
    return TRUE;
}

void on_replace_one_clicked(GtkButton *button, gpointer user_data) {
    if (!search_matches || search_matches->len == 0 || current_match_index < 0) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter *start = &g_array_index(search_matches, GtkTextIter, current_match_index * 2);
    GtkTextIter *end = &g_array_index(search_matches, GtkTextIter, current_match_index * 2 + 1);

    const gchar *replacement = gtk_entry_get_text(GTK_ENTRY(replace_with_entry));
    const gchar *search_text = gtk_entry_get_text(GTK_ENTRY(replace_find_entry));
    if (!search_text || !*search_text) return;

    gtk_text_buffer_delete(buffer, start, end);
    gtk_text_buffer_insert(buffer, start, replacement, -1);

    on_search_activate(NULL, NULL);
}

void on_replace_all_clicked(GtkButton *button, gpointer user_data) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    const gchar *replacement = gtk_entry_get_text(GTK_ENTRY(replace_with_entry));
    const gchar *search_text = gtk_entry_get_text(GTK_ENTRY(replace_find_entry));
    if (!search_text || !*search_text || !replacement) return;

    clear_search_highlights();
    gtk_entry_set_text(GTK_ENTRY(search_entry), search_text);
    on_search_activate(NULL, NULL);

    if (!search_matches || search_matches->len == 0) return;

    GArray *offsets = g_array_new(FALSE, FALSE, sizeof(int));

    for (int i = 0; i < search_matches->len; i++) {
        GtkTextIter iter = g_array_index(search_matches, GtkTextIter, i);
        int offset = gtk_text_iter_get_offset(&iter);
        g_array_append_val(offsets, offset);
    }

    for (int i = offsets->len - 2; i >= 0; i -= 2) {
        int start_offset = g_array_index(offsets, int, i);
        int end_offset = g_array_index(offsets, int, i + 1);

        GtkTextIter start_iter, end_iter;
        gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start_offset);
        gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end_offset);

        gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
        gtk_text_buffer_insert(buffer, &start_iter, replacement, -1);
    }

    g_array_free(offsets, TRUE);
    gtk_entry_set_text(GTK_ENTRY(replace_find_entry), gtk_entry_get_text(GTK_ENTRY(replace_with_entry)));
    gtk_entry_set_text(GTK_ENTRY(replace_with_entry), "");
    on_search_activate(NULL, NULL);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Notebook");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(quit_app), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // top menu bar
    GtkWidget *menu_bar = gtk_menu_bar_new();

    // file dropdown
    GtkWidget *file_menu = gtk_menu_new();

    GtkWidget *file_item = gtk_menu_item_new_with_label("File");
    GtkWidget *new_item = gtk_menu_item_new_with_label("New");
    GtkWidget *open_item = gtk_menu_item_new_with_label("Open");
    GtkWidget *save_item = gtk_menu_item_new_with_label("Save");
    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");

    g_signal_connect(new_item, "activate", G_CALLBACK(new_file), NULL);
    g_signal_connect(open_item, "activate", G_CALLBACK(open_file), window);
    g_signal_connect(save_item, "activate", G_CALLBACK(save_file), window);
    g_signal_connect(quit_item, "activate", G_CALLBACK(quit_app), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), new_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_item);

    // edit dropdown
    GtkWidget *edit_menu = gtk_menu_new();
    GtkWidget *edit_item = gtk_menu_item_new_with_label("Edit");

    GtkWidget *undo_item = gtk_menu_item_new_with_label("Undo");
    GtkWidget *redo_item = gtk_menu_item_new_with_label("Redo");
    GtkWidget *search_button = gtk_menu_item_new_with_label("Find");
    GtkWidget *replace_button = gtk_menu_item_new_with_label("Replace");
    GtkWidget *set_undo_limit_item = gtk_menu_item_new_with_label("Set Max Undo History...");

    g_signal_connect(undo_item, "activate", G_CALLBACK(undo), NULL);
    g_signal_connect(redo_item, "activate", G_CALLBACK(redo), NULL);
    g_signal_connect(search_button, "activate", G_CALLBACK(show_search_bar), NULL);
    g_signal_connect(replace_button, "activate", G_CALLBACK(show_replace_bar), NULL);
    g_signal_connect(set_undo_limit_item, "activate", G_CALLBACK(on_set_undo_limit_activate), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), undo_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), redo_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), search_button);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), replace_button);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), set_undo_limit_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), edit_item);

    // view dropdown
    GtkWidget *view_menu = gtk_menu_new();
    GtkWidget *view_item = gtk_menu_item_new_with_label("View");
    GtkWidget *zoom_in_item = gtk_menu_item_new_with_label("Zoom In");
    GtkWidget *zoom_out_item = gtk_menu_item_new_with_label("Zoom Out");

    g_signal_connect(zoom_in_item, "activate", G_CALLBACK(zoom_in), NULL);
    g_signal_connect(zoom_out_item, "activate", G_CALLBACK(zoom_out), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), zoom_in_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), zoom_out_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), view_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), view_item);

    // bottom info text
    info_text = gtk_label_new(" Characters: 0  Lines: 1  Text Size: 12");
    gtk_label_set_xalign(GTK_LABEL(info_text), 0.0);

    // scrolling for text view
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // text view
    text_view = gtk_text_view_new();
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_apply_tag(buffer, font_tag, &start, &end);

    // font tag
    GtkTextTagTable *tag_table = gtk_text_buffer_get_tag_table(buffer);
    font_tag = gtk_text_tag_new("font_tag");
    gtk_text_tag_table_add(tag_table, font_tag);

    // highlight tag
    highlight_tag = gtk_text_tag_new("highlight");
    g_object_set(highlight_tag, "background", "#DFAF36", NULL);
    gtk_text_tag_table_add(tag_table, highlight_tag);

    g_signal_connect(buffer, "changed", G_CALLBACK(on_text_changed), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);

    // ctrl + f search
    search_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    search_entry = gtk_entry_new();

    case_checkbox = gtk_check_button_new_with_label("Case Sensitive");

    match_label = gtk_label_new("0 matches ");
    find_prev_button = gtk_button_new_with_label("←");
    find_next_button = gtk_button_new_with_label("→");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(case_checkbox), TRUE);    
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Find...");   

    g_signal_connect(search_entry, "activate", G_CALLBACK(on_search_activate), NULL);
    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_entry_text_changed), NULL);
    g_signal_connect(case_checkbox, "toggled", G_CALLBACK(on_search_activate), NULL);
    g_signal_connect(find_prev_button, "clicked", G_CALLBACK(on_find_prev_clicked), NULL);
    g_signal_connect(find_next_button, "clicked", G_CALLBACK(on_find_next_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(search_bar), search_entry, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(search_bar), find_prev_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(search_bar), find_next_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(search_bar), case_checkbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(search_bar), match_label, FALSE, FALSE, 0);

    gtk_box_set_spacing(GTK_BOX(search_bar), 4);

    // ctrl + g replace
    replace_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    replace_match_label = gtk_label_new(" 0 matches ");
    replace_find_entry = gtk_entry_new();
    replace_with_entry = gtk_entry_new();
    replace_one_button = gtk_button_new_with_label("Replace");
    replace_all_button = gtk_button_new_with_label("Replace All");
    replace_find_prev_button = gtk_button_new_with_label("←");
    replace_find_next_button = gtk_button_new_with_label("→");
    replace_case_checkbox = gtk_check_button_new_with_label("Case Sensitive");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(replace_case_checkbox), TRUE);

    gtk_entry_set_placeholder_text(GTK_ENTRY(replace_find_entry), "Find...");
    gtk_entry_set_placeholder_text(GTK_ENTRY(replace_with_entry), "Replace with...");

    g_signal_connect(replace_find_entry, "activate", G_CALLBACK(on_search_activate), NULL);
    g_signal_connect(replace_find_entry, "changed", G_CALLBACK(on_search_entry_text_changed), NULL);
    g_signal_connect(replace_one_button, "clicked", G_CALLBACK(on_replace_one_clicked), NULL);
    g_signal_connect(replace_all_button, "clicked", G_CALLBACK(on_replace_all_clicked), NULL);
    g_signal_connect(replace_find_prev_button, "clicked", G_CALLBACK(on_find_prev_clicked), NULL);
    g_signal_connect(replace_find_next_button, "clicked", G_CALLBACK(on_find_next_clicked), NULL);
    g_signal_connect(replace_case_checkbox, "toggled", G_CALLBACK(on_search_activate), NULL);

    gtk_box_pack_start(GTK_BOX(replace_bar), replace_find_entry, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(replace_bar), replace_with_entry, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(replace_bar), replace_case_checkbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(replace_bar), replace_match_label, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(replace_bar), replace_one_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(replace_bar), replace_all_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(replace_bar), replace_find_prev_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(replace_bar), replace_find_next_button, FALSE, FALSE, 0);

    // assemble gui
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), search_bar, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), replace_bar, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), info_text, FALSE, FALSE, 2);

    // setup
    update_label_text();
    gtk_widget_show_all(window);

    gtk_widget_hide(search_bar);
    gtk_widget_hide(replace_bar);

    push_undo_state();
    gtk_main();

    return 0;
}
