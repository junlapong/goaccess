/** 
 * ui.c -- curses user interface
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 *
 * This program is free software; you can redistribute it and/or    
 * modify it under the terms of the GNU General Public License as   
 * published by the Free Software Foundation; either version 2 of   
 * the License, or (at your option) any later version.              
 *                                                                  
 * This program is distributed in the hope that it will be useful,  
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    
 * GNU General Public License for more details.                     
 *                                                                  
 * A copy of the GNU General Public License is attached to this 
 * source distribution for its full text.
 *
 * Visit http://goaccess.prosoftcorp.com for new releases.
 */

/* "_XOPEN_SOURCE" is required for the GNU libc to export "strptime(3)"
 * correctly. */
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define _XOPEN_SOURCE 700
#define STDIN_FILENO  0

#include <string.h>
#include <curses.h>
#include <time.h>
#include <menu.h>
#include <glib.h>
#include <stdlib.h>
#include <GeoIP.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>

#include "parser.h"
#include "alloc.h"
#include "commons.h"
#include "util.h"
#include "ui.h"

static MENU *my_menu = NULL;
static ITEM **items = NULL;

/* creation - ncurses' window handling */
WINDOW *
create_win (WINDOW * main_win)
{
   int y, x;
   getmaxyx (main_win, y, x);
   return (newwin (y - 12, x - 40, 8, 20));
}

/* delete ncurses window handling */
void
close_win (WINDOW * w)
{
   if (w == NULL)
      return;
   wclear (w);
   wrefresh (w);
   delwin (w);
}

/* get the current date-time */
void
generate_time (void)
{
   now = time (NULL);
   now_tm = localtime (&now);
}

void
draw_header (WINDOW * win, char *header, int x, int y, int w, int color)
{
   if (color_scheme == MONOCHROME) {
      init_pair (1, COLOR_BLACK, COLOR_WHITE);
      init_pair (2, COLOR_WHITE, -1);
   } else {
      init_pair (1, COLOR_BLACK, COLOR_GREEN);
      init_pair (2, COLOR_BLACK, COLOR_CYAN);
   }
   wattron (win, COLOR_PAIR (color));
   mvwhline (win, y, x, ' ', w);
   mvwaddnstr (win, y, x, header, w);
   wattroff (win, COLOR_PAIR (color));
}

void
update_header (WINDOW * header_win, int current)
{
   int row = 0, col = 0;

   getmaxyx (stdscr, row, col);
   wattron (header_win, COLOR_PAIR (BLUE_GREEN));
   wmove (header_win, 0, 30);
   mvwprintw (header_win, 0, col - 20, "[Active Module %d]", current);
   wattroff (header_win, COLOR_PAIR (BLUE_GREEN));
   wrefresh (header_win);
}

void
term_size (WINDOW * main_win)
{
   getmaxyx (stdscr, term_h, term_w);

   real_size_y = term_h - (MAX_HEIGHT_HEADER + MAX_HEIGHT_FOOTER);
   wresize (main_win, real_size_y, term_w);
   wmove (main_win, real_size_y, 0);
}

void
display_general (WINDOW * header_win, struct logger *logger, char *ifile)
{
   int row, col;
   char *head_desc =
      " General Statistics - Information analyzed from log file - Unique totals";
   getmaxyx (stdscr, row, col);
   draw_header (header_win, head_desc, 0, 0, col, 1);

   /* general stats */
   wattron (header_win, A_BOLD | COLOR_PAIR (COL_CYAN));
   mvwprintw (header_win, 2, 18, "%u", logger->total_process);
   mvwprintw (header_win, 3, 18, "%u", logger->total_invalid);
   mvwprintw (header_win, 4, 18, "%d sec", (int) end_proc - start_proc);
   mvwprintw (header_win, 2, 50, "%d",
              g_hash_table_size (ht_unique_visitors));
   mvwprintw (header_win, 3, 50, "%d", g_hash_table_size (ht_requests));
   mvwprintw (header_win, 4, 50, "%d",
              g_hash_table_size (ht_requests_static));
   mvwprintw (header_win, 2, 75, "%d", g_hash_table_size (ht_referrers));
   mvwprintw (header_win, 3, 75, "%d",
              g_hash_table_size (ht_not_found_requests));

   off_t log_size = file_size (ifile);
   char *size = filesize_str (log_size);
   mvwprintw (header_win, 2, 87, "%s", size);
   free (size);

   if (bandwidth_flag) {
      char *bw = filesize_str ((float) req_size);
      mvwprintw (header_win, 3, 87, "%s", bw);
      free (bw);
   } else
      mvwprintw (header_win, 3, 87, "N/A");

   wattroff (header_win, A_BOLD | COLOR_PAIR (COL_CYAN));
   wattron (header_win, COLOR_PAIR (COL_YELLOW));
   mvwprintw (header_win, 4, 58, "%s", ifile);
   wattroff (header_win, COLOR_PAIR (COL_YELLOW));

   /* labels */
   wattron (header_win, COLOR_PAIR (COL_WHITE));
   mvwprintw (header_win, 2, 2, "Total Requests");
   mvwprintw (header_win, 3, 2, "Failed Requests");
   mvwprintw (header_win, 4, 2, "Generation Time");
   mvwprintw (header_win, 2, 28, "Total Unique Visitors");
   mvwprintw (header_win, 3, 28, "Total Unique Files");
   mvwprintw (header_win, 4, 28, "Total Static Files");
   mvwprintw (header_win, 2, 58, "Total Referrers");
   mvwprintw (header_win, 3, 58, "Total Unique 404");
   mvwprintw (header_win, 3, 82, "BW");
   mvwprintw (header_win, 2, 82, "Log");
   wattroff (header_win, COLOR_PAIR (COL_WHITE));
}

void
create_graphs (WINDOW * main_win, struct struct_display **s_display,
               struct logger *logger, int i, int module, int max)
{
   struct tm tm;
   int x, y, xx, r, col, row;
   float l_bar, scr_cal, orig_cal;
   char buf[12] = "";           /* date */

   GHashTable *hash_table = NULL;

   memset (&tm, 0, sizeof (tm));

   getyx (main_win, y, x);
   getmaxyx (stdscr, row, col);

   switch (module) {
    case UNIQUE_VISITORS:
       hash_table = ht_unique_visitors;
       break;
    case HOSTS:
       hash_table = ht_hosts;
       break;
    case STATUS_CODES:
       hash_table = ht_status_code;
       break;
   }

   int inc_pos_x = 0, magn = 0;

   magn = floor (log10 (logger->total_process));
   if (magn >= 7)
      inc_pos_x = magn % 5;
   else
      inc_pos_x = 0;

   orig_cal = (float) (s_display[i]->hits * 100);
   l_bar = (float) (s_display[i]->hits * 100);

   /* hosts - status codes */
   if (module != 8 && module != 9)
      orig_cal = (l_bar / g_hash_table_size (hash_table));
   else
      orig_cal = (orig_cal / logger->total_process);

   l_bar = (l_bar / max);
   /* unique visitors */
   if (s_display[i]->module == 1) {
      strptime (s_display[i]->data, "%Y%m%d", &tm);
      strftime (buf, sizeof (buf), "%d/%b/%Y", &tm);
      mvwprintw (main_win, y, 18 + inc_pos_x, "%s", buf);
   } else if (s_display[i]->module == 9)
      /* HTTP status codes */
      mvwprintw (main_win, y, 18 + inc_pos_x, "%s\t%s",
                 s_display[i]->data, verify_status_code (s_display[i]->data));
   else
      mvwprintw (main_win, y, 18 + inc_pos_x, "%s", s_display[i]->data);
   mvwprintw (main_win, y, 2, "%d", s_display[i]->hits);
   if (s_display[i]->hits == max)
      wattron (main_win, COLOR_PAIR (COL_YELLOW));
   else
      wattron (main_win, COLOR_PAIR (COL_RED));
   mvwprintw (main_win, y, 10 + inc_pos_x, "%4.2f%%", orig_cal);
   if (s_display[i]->hits == max)
      wattroff (main_win, COLOR_PAIR (COL_YELLOW));
   else
      wattroff (main_win, COLOR_PAIR (COL_RED));
   if (s_display[i]->module == 9)
      return;

   scr_cal = (float) ((col - 38) - inc_pos_x);
   scr_cal = (float) scr_cal / 100;
   l_bar = l_bar * scr_cal;

   wattron (main_win, COLOR_PAIR (COL_GREEN));
   for (r = 0, xx = 35 + inc_pos_x; r < (int) l_bar; r++, xx++)
      mvwprintw (main_win, y, xx, "|");
   wattroff (main_win, COLOR_PAIR (COL_GREEN));
}

int
get_max_value (struct struct_display **s_display, struct logger *logger,
               int module)
{
   int i, temp = 0;
   for (i = 0; i < logger->alloc_counter; i++) {
      if (s_display[i]->module == module) {
         if (s_display[i]->hits > temp)
            temp = s_display[i]->hits;
      }
   }
   return temp;
}

static void
data_by_total_hits (WINDOW * main_win, int pos_y, struct logger *logger,
                    struct struct_display **s_display, int i)
{
   float h = 0, t = 0;
   int inc_pos_x = 0, magn = 0, col, row;
   getmaxyx (main_win, row, col);

   if (s_display[i]->hits == 0)
      return;

   h = (s_display[i]->hits * 100);
   t = (h / logger->total_process);

   magn = floor (log10 (logger->total_process));
   if (magn >= 7)
      inc_pos_x = magn % 5;
   else
      inc_pos_x = 0;

   gpointer value_ptr;
   long long *ptr_value;

   if (bandwidth_flag
       && (s_display[i]->module == REQUESTS
           || s_display[i]->module == REQUESTS_STATIC)) {
      value_ptr = g_hash_table_lookup (ht_file_bw, s_display[i]->data);
      if (value_ptr != NULL) {
         ptr_value = (long long *) value_ptr;
         char *bw = filesize_str (*ptr_value);
         wattron (main_win, A_BOLD | COLOR_PAIR (COL_BLACK));
         mvwprintw (main_win, pos_y, 18 + inc_pos_x, "%9s", bw);
         wattroff (main_win, A_BOLD | COLOR_PAIR (COL_BLACK));
         if (strlen (s_display[i]->data) > ((size_t) (col - 32))) {
            char *str = (char *) malloc (col - 32 + 1);
            if (str == NULL)
               error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                              "Unable to allocate memory");
            strncpy (str, s_display[i]->data, col - 32);
            (str)[col - 32] = 0;
            mvwprintw (main_win, pos_y, 29 + inc_pos_x, "%s", str);
            free (str);
         } else
            mvwprintw (main_win, pos_y, 29 + inc_pos_x, "%s",
                       s_display[i]->data);
         free (bw);
      }
   } else if (strlen (s_display[i]->data) > ((size_t) (col - 22))) {
      char *str = (char *) malloc (col - 22 + 1);
      if (str == NULL)
         error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                        "Unable to allocate memory");
      strncpy (str, s_display[i]->data, col - 22);
      (str)[col - 22] = 0;
      mvwprintw (main_win, pos_y, 18 + inc_pos_x, "%s", str);
      free (str);
   } else
      mvwprintw (main_win, pos_y, 18 + inc_pos_x, "%s", s_display[i]->data);

   mvwprintw (main_win, pos_y, 2, "%d", s_display[i]->hits);

   wattron (main_win, A_BOLD | COLOR_PAIR (COL_BLACK));
   mvwprintw (main_win, pos_y, 10 + inc_pos_x, "%4.2f%%", t);
   wattroff (main_win, A_BOLD | COLOR_PAIR (COL_BLACK));
}

/* ###NOTE: Modules 6, 7 are based on module 1 totals 
   this way we avoid the overhead of adding them up */
void
display_content (WINDOW * main_win, struct struct_display **s_display,
                 struct logger *logger, struct scrolling scrolling)
{
   int i, x, y, max = 0, until = 0, start = 0, pos_y = 0;

   getmaxyx (stdscr, term_h, term_w);
   getmaxyx (main_win, y, x);

   if (term_h < MIN_HEIGHT || term_w < MIN_WIDTH)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Minimum screen size - 97 columns by 40 lines");

   if (logger->alloc_counter > real_size_y)
      until = real_size_y + scrolling.init_scrl_main_win;
   else
      logger->alloc_counter;

   start = scrolling.init_scrl_main_win;

   /* making sure we dont go over logger->alloc_counter */
   if (until > logger->alloc_counter)
      until = logger->alloc_counter;

   for (i = start; i < until; i++, pos_y++) {
      if (s_display[i]->hits != 0)
         mvwprintw (main_win, pos_y, 2, "%d", s_display[i]->hits);
      /* draw headers */
      if ((i % 10) == 0)
         draw_header (main_win, s_display[i]->data, 0, pos_y, x, 1);
      else if ((i % 10) == 1) {
         draw_header (main_win, s_display[i]->data, 0, pos_y, x, 2);
      } else if (((s_display[i]->module == UNIQUE_VISITORS))
                 && ((i % 10 >= 3) && (i % 10 <= 8)
                     && (s_display[i]->hits != 0))) {
         max = get_max_value (s_display, logger, UNIQUE_VISITORS);
         create_graphs (main_win, s_display, logger, i, 1, max);

      } else if (((s_display[i]->module == OS))
                 && ((i % 10 >= 3) && (i % 10 <= 8)
                     && (s_display[i]->hits != 0))) {
         max = get_max_value (s_display, logger, OS);
         create_graphs (main_win, s_display, logger, i, 1, max);

      } else if (((s_display[i]->module == BROWSERS))
                 && ((i % 10 >= 3) && (i % 10 <= 8)
                     && (s_display[i]->hits != 0))) {
         max = get_max_value (s_display, logger, BROWSERS);
         create_graphs (main_win, s_display, logger, i, 1, max);

      } else if (((s_display[i]->module == HOSTS))
                 && ((i % 10 >= 3) && (i % 10 <= 8)
                     && (s_display[i]->hits != 0))) {
         max = get_max_value (s_display, logger, HOSTS);
         create_graphs (main_win, s_display, logger, i, HOSTS, max);

      } else if (((s_display[i]->module == STATUS_CODES))
                 && ((i % 10 >= 3) && (i % 10 <= 8)
                     && (s_display[i]->hits != 0))) {
         max = get_max_value (s_display, logger, STATUS_CODES);
         create_graphs (main_win, s_display, logger, i, STATUS_CODES, max);

      } else
         data_by_total_hits (main_win, pos_y, logger, s_display, i);
   }
}

/* ###NOTE: Modules 6, 7 are based on module 1 totals 
   this way we avoid the overhead of adding them up */
void
do_scrolling (WINDOW * main_win, struct struct_display **s_display,
              struct logger *logger, struct scrolling *scrolling, int cmd)
{
   int cur_y, cur_x, y, x, max = 0;
   getyx (main_win, cur_y, cur_x);      /* cursor */
   getmaxyx (main_win, y, x);

   int i = real_size_y + scrolling->init_scrl_main_win;
   int j = scrolling->init_scrl_main_win - 1;

   switch (cmd) {
       /* scroll down main window */
    case 1:
       if (!(i < logger->alloc_counter))
          return;
       scrollok (main_win, TRUE);
       wscrl (main_win, 1);
       scrollok (main_win, FALSE);

       if (s_display[i]->hits != 0)
          mvwprintw (main_win, cur_y, 2, "%d", s_display[i]->hits);
       /* draw headers */
       if ((i % 10) == 0)
          draw_header (main_win, s_display[i]->data, 0, cur_y, x, 1);
       else if ((i % 10) == 1) {
          draw_header (main_win, s_display[i]->data, 0, cur_y, x, 2);
       } else if (((s_display[i]->module == UNIQUE_VISITORS))
                  && ((i % 10 >= 3) && (i % 10 <= 8)
                      && (s_display[i]->hits != 0))) {
          max = get_max_value (s_display, logger, UNIQUE_VISITORS);
          create_graphs (main_win, s_display, logger, i, 1, max);

       } else if (((s_display[i]->module == OS))
                  && ((i % 10 >= 3) && (i % 10 <= 8)
                      && (s_display[i]->hits != 0))) {
          max = get_max_value (s_display, logger, OS);
          create_graphs (main_win, s_display, logger, i, 1, max);

       } else if (((s_display[i]->module == BROWSERS))
                  && ((i % 10 >= 3) && (i % 10 <= 8)
                      && (s_display[i]->hits != 0))) {
          max = get_max_value (s_display, logger, BROWSERS);
          create_graphs (main_win, s_display, logger, i, 1, max);

       } else if (((s_display[i]->module == HOSTS))
                  && ((i % 10 >= 3) && (i % 10 <= 8)
                      && (s_display[i]->hits != 0))) {
          max = get_max_value (s_display, logger, HOSTS);
          create_graphs (main_win, s_display, logger, i, 8, max);

       } else if (((s_display[i]->module == STATUS_CODES))
                  && ((i % 10 >= 3) && (i % 10 <= 8)
                      && (s_display[i]->hits != 0))) {
          max = get_max_value (s_display, logger, STATUS_CODES);
          create_graphs (main_win, s_display, logger, i, 9, max);

       } else
          data_by_total_hits (main_win, cur_y, logger, s_display, i);

       scrolling->scrl_main_win++;
       scrolling->init_scrl_main_win++;
       break;
       /* scroll up main window */
    case 0:
       if (!(j >= 0))
          return;
       scrollok (main_win, TRUE);
       wscrl (main_win, -1);
       scrollok (main_win, FALSE);

       if (s_display[j]->hits != 0)
          mvwprintw (main_win, 0, 2, "%d", s_display[j]->hits);
       /* draw headers */
       if ((j % 10) == 0)
          draw_header (main_win, s_display[j]->data, 0, 0, x, 1);
       else if ((j % 10) == 1) {
          draw_header (main_win, s_display[j]->data, 0, 0, x, 2);
       } else if (((s_display[j]->module == UNIQUE_VISITORS))
                  && ((j % 10 >= 3) && (j % 10 <= 8)
                      && (s_display[j]->hits != 0))) {
          max = get_max_value (s_display, logger, UNIQUE_VISITORS);
          create_graphs (main_win, s_display, logger, j, 1, max);
       } else if (((s_display[j]->module == OS))
                  && ((j % 10 >= 3) && (j % 10 <= 8)
                      && (s_display[j]->hits != 0))) {
          max = get_max_value (s_display, logger, OS);
          create_graphs (main_win, s_display, logger, j, 1, max);
       } else if (((s_display[j]->module == BROWSERS))
                  && ((j % 10 >= 3) && (j % 10 <= 8)
                      && (s_display[j]->hits != 0))) {
          max = get_max_value (s_display, logger, BROWSERS);
          create_graphs (main_win, s_display, logger, j, 1, max);
       } else if (((s_display[j]->module == HOSTS))
                  && ((j % 10 >= 3) && (j % 10 <= 8)
                      && (s_display[j]->hits != 0))) {
          max = get_max_value (s_display, logger, HOSTS);
          create_graphs (main_win, s_display, logger, j, 8, max);

       } else if (((s_display[j]->module == STATUS_CODES))
                  && ((j % 10 >= 3) && (j % 10 <= 8)
                      && (s_display[j]->hits != 0))) {
          max = get_max_value (s_display, logger, STATUS_CODES);
          create_graphs (main_win, s_display, logger, j, 9, max);

       } else
          data_by_total_hits (main_win, cur_y, logger, s_display, j);

       scrolling->scrl_main_win--;
       scrolling->init_scrl_main_win--;
       break;
   }
}

static void
load_help_popup_content (WINDOW * inner_win, int where,
                         struct scrolling *scrolling)
{
   int y, x;
   getmaxyx (inner_win, y, x);

   switch (where) {
       /* scroll down */
    case 1:
       if (((size_t) (scrolling->scrl_help_win - 5)) >= help_main_size ())
          return;
       scrollok (inner_win, TRUE);
       wscrl (inner_win, 1);
       scrollok (inner_win, FALSE);
       wmove (inner_win, y - 1, 2);
       /* minus help_win offset - 5 */
       mvwaddstr (inner_win, y - 1, 2,
                  help_main[scrolling->scrl_help_win - 5]);
       scrolling->scrl_help_win++;
       break;
       /* scroll up */
    case 0:
       if ((scrolling->scrl_help_win - y) - 5 <= 0)
          return;
       scrollok (inner_win, TRUE);
       wscrl (inner_win, -1);
       scrollok (inner_win, FALSE);
       wmove (inner_win, 0, 2);
       /* minus help_win offset - 6 */
       mvwaddstr (inner_win, 0, 2,
                  help_main[(scrolling->scrl_help_win - y) - 6]);
       scrolling->scrl_help_win--;
       break;
   }
   wrefresh (inner_win);
}

void
load_help_popup (WINDOW * help_win)
{
   WINDOW *inner_win;
   int y, x, c, quit = 1;
   size_t sz;
   struct scrolling scrolling;

   getmaxyx (help_win, y, x);
   draw_header (help_win,
                "  Use cursor UP/DOWN - PGUP/PGDOWN to scroll. q:quit", 0, 1,
                x, 2);
   wborder (help_win, '|', '|', '-', '-', '+', '+', '+', '+');
   inner_win = newwin (y - 5, x - 4, 11, 21);
   sz = help_main_size ();

   int i, m = 0;
   for (i = 0; (i < y) && (((size_t) i) < sz); i++, m++)
      mvwaddstr (inner_win, m, 2, help_main[i]);

   scrolling.scrl_help_win = y;
   wmove (help_win, y, 0);
   wrefresh (help_win);
   wrefresh (inner_win);

   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case KEY_DOWN:
          (void) load_help_popup_content (inner_win, 1, &scrolling);
          break;
       case KEY_UP:
          (void) load_help_popup_content (inner_win, 0, &scrolling);
          break;
       case KEY_RESIZE:
       case 'q':
          quit = 0;
          break;
      }
      wrefresh (help_win);
   }
   render_screens ();
}

void
load_reverse_dns_popup (WINDOW * ip_detail_win, char *addr)
{
   int y, x, c, quit = 1;
   char *my_addr = reverse_ip (addr);
   const char *location;

   getmaxyx (ip_detail_win, y, x);
   draw_header (ip_detail_win, "  Reverse DNS lookup - q:quit", 0, 1, x - 1,
                2);
   wborder (ip_detail_win, '|', '|', '-', '-', '+', '+', '+', '+');
   mvwprintw (ip_detail_win, 3, 2, "Reverse DNS for address: %s", addr);
   mvwprintw (ip_detail_win, 4, 2, "%s", my_addr);

   /* geolocation data */
   GeoIP *gi;
   gi = GeoIP_new (GEOIP_STANDARD);
   location = GeoIP_country_name_by_name (gi, addr);
   GeoIP_delete (gi);
   if (location == NULL)
      location = "Not found";
   mvwprintw (ip_detail_win, 5, 2, "Country: %s", location);
   free (my_addr);

   wrefresh (ip_detail_win);
   /* ###TODO: resize child windows. */
   /* for now we can close them up */
   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case KEY_RESIZE:
       case 'q':
          quit = 0;
          break;
      }
   }
   render_screens ();
   return;
}

static void
scheme_chosen (char *name)
{
   if (strcmp ("Monochrome/Default", name) == 0)
      color_scheme = MONOCHROME;
   else
      color_scheme = STD_GREEN;
   init_colors ();
   render_screens ();
}

void
load_schemes_win (WINDOW * schemes_win)
{
   int y, x, c, quit = 1, n_choices, i;
   char *choices[] = { "Monochrome/Default", "Green/Original" };
   MENU *menu;
   ITEM **my_items;
   ITEM *cur_item;

   /* Create items */
   n_choices = ARRAY_SIZE (choices);
   my_items = (ITEM **) malloc (sizeof (ITEM *) * (n_choices + 1));
   for (i = 0; i < n_choices; ++i) {
      my_items[i] = new_item (choices[i], choices[i]);
      set_item_userptr (my_items[i], scheme_chosen);
   }
   my_items[n_choices] = (ITEM *) NULL;

   /* crate menu */
   menu = new_menu (my_items);
   menu_opts_off (menu, O_SHOWDESC);
   keypad (schemes_win, TRUE);

   /* set main window and sub window */
   set_menu_win (menu, schemes_win);
   set_menu_sub (menu, derwin (schemes_win, 6, 38, 3, 1));
   set_menu_format (menu, 5, 1);
   set_menu_mark (menu, " => ");

   getmaxyx (schemes_win, y, x);
   draw_header (schemes_win, "  Color schemes - q:quit", 0, 1, x - 1, 2);
   wborder (schemes_win, '|', '|', '-', '-', '+', '+', '+', '+');

   post_menu (menu);
   wrefresh (schemes_win);
   /* ###TODO: resize child windows. */
   /* for now we can close them up */
   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case KEY_DOWN:
          menu_driver (menu, REQ_DOWN_ITEM);
          break;
       case KEY_UP:
          menu_driver (menu, REQ_UP_ITEM);
          break;
       case 32:
       case 0x0a:
       case 0x0d:
       case KEY_ENTER:
          cur_item = current_item (menu);
          if (cur_item == NULL)
             break;
          void (*p) (char *);
          p = item_userptr (cur_item);
          p ((char *) item_name (cur_item));
          pos_menu_cursor (menu);
          quit = 0;
          break;
       case KEY_RESIZE:
       case 'q':
          quit = 0;
          break;
      }
      wrefresh (schemes_win);
   }
   unpost_menu (menu);
   free_menu (menu);
   for (i = 0; i < n_choices; ++i) {
      free_item (my_items[i]);
   }
   free (my_items);
   render_screens ();
   return;
}

static char *
get_browser_type (char *line)
{
   char *p;
   char token[128];

   if (p = strchr (line, '|')) {
      if (sscanf (line, "%[^|]", token) == 1)
         return line + strlen (token) + 1;
      else {
         token[0] = '\0';
         return p + 1;
      }
   } else
      return "Others";
}

static char *
convert_hits_to_string (int nhits)
{
   char *hits = NULL;
   hits = (char *) malloc (sizeof (char) * 11);
   if (hits == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory");
   sprintf (hits, "%3i", nhits);
   return hits;
}

static ITEM **
get_menu_items (struct struct_holder **s_holder, struct logger *logger,
                int choices, int sort)
{
   int i;
   char *hits = NULL;
   char buf[12] = "";
   char *buffer_date = NULL, *b_version = NULL, *o_version = NULL;
   char *p = NULL, *b;
   struct tm tm;
   ITEM **items;

   /* requests module */
   gpointer value_ptr;
   long long *ptr_value;
   char *bw, *w_bw, *status_code, *token, *status_str;

   memset (&tm, 0, sizeof (tm));

   /* sort struct prior to display */
   if (sort)
      qsort (s_holder, logger->counter, sizeof (struct struct_holder *),
             struct_cmp_by_hits);
   else
      qsort (s_holder, logger->counter, sizeof (struct struct_holder *),
             struct_cmp_desc);

   if (logger->current_module == BROWSERS || logger->current_module == OS)
      qsort (s_holder, logger->counter, sizeof (struct struct_holder *),
             struct_cmp_asc);

   items = (ITEM **) malloc (sizeof (ITEM *) * (choices + 1));

   for (i = 0; i < choices; ++i) {
      switch (logger->current_module) {
       case UNIQUE_VISITORS:
          hits = convert_hits_to_string (s_holder[i]->hits);
          buffer_date = (char *) malloc (sizeof (char) * 13);
          if (buffer_date == NULL)
             error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                            "Unable to allocate memory");
          strptime (s_holder[i]->data, "%Y%m%d", &tm);
          strftime (buf, sizeof (buf), "%d/%b/%Y", &tm);
          sprintf (buffer_date, "%s", buf);
          items[i] = new_item (hits, buffer_date);
          break;
       case REQUESTS:
       case REQUESTS_STATIC:
          if (!bandwidth_flag) {
             hits = convert_hits_to_string (s_holder[i]->hits);
             items[i] = new_item (hits, s_holder[i]->data);
             break;
          }
          value_ptr = g_hash_table_lookup (ht_file_bw, s_holder[i]->data);
          if (value_ptr == NULL)
             break;
          ptr_value = (long long *) value_ptr;
          bw = filesize_str (*ptr_value);
          w_bw = malloc (snprintf (NULL, 0, "%9s - %d", bw, s_holder[i]->hits)
                         + 2);
          if (w_bw == NULL)
             error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                            "Unable to allocate memory");
          sprintf (w_bw, "%9s - %d", bw, s_holder[i]->hits);
          items[i] = new_item (w_bw, s_holder[i]->data);
          free (bw);
          break;
       case OS:
          hits = convert_hits_to_string (s_holder[i]->hits);
          if (strchr (s_holder[i]->data, '|') == NULL) {
             items[i] = new_item (hits, alloc_string (s_holder[i]->data));
             break;
          }
          o_version =
             malloc (snprintf
                     (NULL, 0, "|`- %s",
                      get_browser_type (s_holder[i]->data)) + 1);
          if (o_version == NULL)
             error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                            "Unable to allocate memory");
          sprintf (o_version, "|`- %s", get_browser_type (s_holder[i]->data));
          items[i] = new_item (hits, o_version);
          break;
       case BROWSERS:
          hits = convert_hits_to_string (s_holder[i]->hits);
          if (strchr (s_holder[i]->data, '|') == NULL) {
             items[i] = new_item (hits, alloc_string (s_holder[i]->data));
             break;
          }
          b_version =
             malloc (snprintf
                     (NULL, 0, "|`- %s",
                      get_browser_type (s_holder[i]->data)) + 1);
          if (b_version == NULL)
             error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                            "Unable to allocate memory");
          sprintf (b_version, "|`- %s", get_browser_type (s_holder[i]->data));
          items[i] = new_item (hits, b_version);
          break;
       case STATUS_CODES:
          hits = convert_hits_to_string (s_holder[i]->hits);
          status_code = verify_status_code (s_holder[i]->data);
          token = (char *) malloc (sizeof (char) * 64);
          if (token == NULL)
             error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                            "Unable to allocate memory");
          if (sscanf (status_code, "%[^-]", token) == 1) {
             status_str =
                malloc (snprintf
                        (NULL, 0, "%s %s", s_holder[i]->data, token) + 1);
             if (status_str == NULL)
                error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                               "Unable to allocate memory");
             sprintf (status_str, "%s %s", s_holder[i]->data, token);
             items[i] = new_item (hits, status_str);
             free (token);
          } else
             items[i] = new_item (hits, alloc_string (s_holder[i]->data));
          break;
       default:
          hits = convert_hits_to_string (s_holder[i]->hits);
          items[i] = new_item (hits, s_holder[i]->data);
          break;
      }
   }
   items[i] = (ITEM *) NULL;

   return items;
}

static MENU *
set_menu (WINDOW * my_menu_win, ITEM ** items, struct logger *logger)
{
   MENU *my_menu = NULL;
   int x = 0;
   int y = 0;

   getmaxyx (my_menu_win, y, x);
   my_menu = new_menu (items);
   keypad (my_menu_win, TRUE);

   /* set main window and sub window */
   set_menu_win (my_menu, my_menu_win);
   set_menu_sub (my_menu, derwin (my_menu_win, y - 6, x - 2, 4, 1));
   set_menu_format (my_menu, y - 6, 1);

   /* set menu mark */
   set_menu_mark (my_menu, " => ");
   draw_header (my_menu_win,
                "  Use cursor UP/DOWN - PGUP/PGDOWN to scroll. q:quit", 0, 1,
                x, 2);
   draw_header (my_menu_win, module_names[logger->current_module - 1], 0, 2,
                x, 1);
   wborder (my_menu_win, '|', '|', '-', '-', '+', '+', '+', '+');
   return my_menu;
}

static void
load_popup_content (WINDOW * my_menu_win, int choices,
                    struct struct_holder **s_holder, struct logger *logger,
                    int sort)
{
   wclrtoeol (my_menu_win);
   items = get_menu_items (s_holder, logger, choices, sort);
   my_menu = set_menu (my_menu_win, items, logger);
   post_menu (my_menu);
   wrefresh (my_menu_win);
}

static void
load_popup_free_items (ITEM ** items, struct logger *logger)
{
   int i;
   char *name = NULL;
   char *description = NULL;

   /* clean up stuff */
   i = 0;
   while ((ITEM *) NULL != items[i]) {
      name = (char *) item_name (items[i]);
      free (name);
      if (logger->current_module == UNIQUE_VISITORS ||
          logger->current_module == OS ||
          logger->current_module == BROWSERS ||
          logger->current_module == STATUS_CODES) {
         description = (char *) item_description (items[i]);
         free (description);
      }
      free_item (items[i]);
      i++;
   }
   free (items);
   items = NULL;
}

static ITEM *
search_request (MENU * my_menu, const char *input)
{
   const char *haystack;
   ITEM *item_ptr = NULL;

   if (input != NULL) {
      int i = -1, j = -1, response = 0;
      j = item_index (current_item (my_menu));

      for (i = j + 1; i < item_count (my_menu) && !response; i++) {
         haystack = item_description (menu_items (my_menu)[i]);
         if (haystack != NULL && input != NULL) {
            if (strstr (haystack, input))
               response = 1;
         } else
            response = 0;
      }
      if (response)
         item_ptr = menu_items (my_menu)[i - 1];
   }
   return item_ptr;
}

void
load_popup (WINDOW * my_menu_win, struct struct_holder **s_holder,
            struct logger *logger)
{
   WINDOW *ip_detail_win;
   ITEM *query = NULL;

   /*###TODO: perhaps let the user change the size of MAX_CHOICES */
   int choices = MAX_CHOICES, c, x, y;
   char input[BUFFER] = "";

   GHashTable *hash_table = NULL;

   switch (logger->current_module) {
    case UNIQUE_VISITORS:
       hash_table = ht_unique_vis;
       break;
    case REQUESTS:
       hash_table = ht_requests;
       break;
    case REQUESTS_STATIC:
       hash_table = ht_requests_static;
       break;
    case REFERRERS:
       hash_table = ht_referrers;
       break;
    case NOT_FOUND:
       hash_table = ht_not_found_requests;
       break;
    case OS:
       hash_table = ht_os;
       break;
    case BROWSERS:
       hash_table = ht_browsers;
       break;
    case HOSTS:
       hash_table = ht_hosts;
       break;
    case STATUS_CODES:
       hash_table = ht_status_code;
       break;
    case REFERRING_SITES:
       hash_table = ht_referring_sites;
       break;
    case KEYPHRASES:
       hash_table = ht_keyphrases;
       break;
   }
   getmaxyx (my_menu_win, y, x);
   MALLOC_STRUCT (s_holder, g_hash_table_size (hash_table));

   int i = 0, quit = 1;
   char *p;
   GHashTableIter iter;
   gpointer k = NULL;
   gpointer v = NULL;

   g_hash_table_iter_init (&iter, hash_table);
   while (g_hash_table_iter_next (&iter, &k, &v)) {
      s_holder[i]->data = (gchar *) k;
      s_holder[i++]->hits = GPOINTER_TO_INT (v);
      logger->counter++;
   }

   /* again, letting the user to set the max number 
    * might be a better way to go */
   if (logger->current_module == BROWSERS)
      choices = g_hash_table_size (hash_table);
   else if (g_hash_table_size (hash_table) > 300)
      choices = MAX_CHOICES;
   else
      choices = g_hash_table_size (hash_table);

   load_popup_content (my_menu_win, choices, s_holder, logger, 1);

   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case KEY_DOWN:
          menu_driver (my_menu, REQ_DOWN_ITEM);
          break;
       case KEY_UP:
          menu_driver (my_menu, REQ_UP_ITEM);
          break;
       case KEY_NPAGE:
          menu_driver (my_menu, REQ_SCR_DPAGE);
          break;
       case KEY_PPAGE:
          menu_driver (my_menu, REQ_SCR_UPAGE);
          break;
       case '/':
          /* set the whole ui for search */
          wattron (my_menu_win, COLOR_PAIR (COL_CYAN));
          mvwhline (my_menu_win, y - 2, 2, ' ', x - 4);
          mvwaddnstr (my_menu_win, y - 2, 2, "/", 20);
          wattroff (my_menu_win, COLOR_PAIR (COL_CYAN));
          nocbreak ();
          echo ();
          curs_set (1);
          wattron (my_menu_win, COLOR_PAIR (COL_CYAN));
          wscanw (my_menu_win, "%s", input);
          wattroff (my_menu_win, COLOR_PAIR (COL_CYAN));
          cbreak ();
          noecho ();
          halfdelay (10);
          nonl ();
          intrflush (stdscr, FALSE);
          curs_set (0);

          query = search_request (my_menu, input);
          if (query != NULL) {
             while (FALSE == item_visible (query))
                menu_driver (my_menu, REQ_SCR_DPAGE);
             set_current_item (my_menu, query);
          } else {
             wattron (my_menu_win, COLOR_PAIR (WHITE_RED));
             mvwhline (my_menu_win, y - 2, 2, ' ', x - 4);
             mvwaddnstr (my_menu_win, y - 2, 2, "Pattern not found", 20);
             wattroff (my_menu_win, COLOR_PAIR (WHITE_RED));
          }
          break;
       case 'n':
          if (strlen (input) == 0)
             break;
          query = search_request (my_menu, input);
          if (query != NULL) {
             while (FALSE == item_visible (query))
                menu_driver (my_menu, REQ_SCR_DPAGE);
             set_current_item (my_menu, query);
          } else {
             wattron (my_menu_win, COLOR_PAIR (WHITE_RED));
             mvwhline (my_menu_win, y - 2, 2, ' ', x - 4);
             mvwaddnstr (my_menu_win, y - 2, 2, "search hit BOTTOM", 20);
             wattroff (my_menu_win, COLOR_PAIR (WHITE_RED));
          }
          break;
       case 116:
          menu_driver (my_menu, REQ_FIRST_ITEM);
          break;
       case 98:
          menu_driver (my_menu, REQ_LAST_ITEM);
          break;
       case 's':
          if (logger->current_module != UNIQUE_VISITORS)
             break;
          unpost_menu (my_menu);
          free_menu (my_menu);
          my_menu = NULL;
          load_popup_free_items (items, logger);
          load_popup_content (my_menu_win, choices, s_holder, logger, 0);
          break;
       case 'S':
          if (logger->current_module != UNIQUE_VISITORS)
             break;
          unpost_menu (my_menu);
          free_menu (my_menu);
          my_menu = NULL;
          load_popup_free_items (items, logger);
          load_popup_content (my_menu_win, choices, s_holder, logger, 1);
          break;
       case 10:
       case KEY_RIGHT:
          if (logger->current_module != 8)
             break;

          ITEM *cur;
          cur = current_item (my_menu);
          if (cur == NULL)
             break;

          ip_detail_win = newwin (y - 13, x - 5, 10, 21);
          char addrs[32];
          sprintf (addrs, "%s", item_description (cur));
          load_reverse_dns_popup (ip_detail_win, addrs);
          pos_menu_cursor (my_menu);
          wrefresh (ip_detail_win);
          touchwin (my_menu_win);
          close_win (ip_detail_win);
          break;
          /* ###TODO: resize child windows. */
          /* for now we can close them up */
       case KEY_RESIZE:
       case 'q':
          quit = 0;
          break;
      }
      wrefresh (my_menu_win);
   }

   int f;
   for (f = 0; f < logger->counter; f++)
      free (s_holder[f]);
   free (s_holder);
   logger->counter = 0;

   /* unpost and free all the memory taken up */
   unpost_menu (my_menu);
   free_menu (my_menu);

   /* clean up stuff */
   my_menu = NULL;
   load_popup_free_items (items, logger);
   render_screens ();
}
