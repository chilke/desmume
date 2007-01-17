/* callbacks_IO.c - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Damien Nozay (damdoum)
 * Author: damdoum at users.sourceforge.net
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "callbacks_IO.h"

static u16 Cur_Keypad = 0;
int ScreenCoeff_Size=1;
gboolean ScreenRotate=FALSE;
gboolean Boost=FALSE;
int BoostFS=20;
int saveFS;

/* ***** ***** INPUT BUTTONS / KEYBOARD ***** ***** */
gboolean  on_wMainW_key_press_event    (GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	u16 Key = lookup_key(event->keyval);
	if (event->keyval == keyboard_cfg[KEY_BOOST-1]) {
		Boost != Boost;
		saveFS=Frameskip;
		if (Boost) Frameskip = BoostFS;
		else Frameskip = saveFS;
	}
        ADD_KEY( Cur_Keypad, Key );
        if(desmume_running()) update_keypad(Cur_Keypad);
	return 1;
}

gboolean  on_wMainW_key_release_event  (GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	u16 Key = lookup_key(event->keyval);
        RM_KEY( Cur_Keypad, Key );
        if(desmume_running()) update_keypad(Cur_Keypad);
	return 1;
}



/* ***** ***** SCREEN DRAWING ***** ***** */
int has_pix_col_map=0;
u32 pix_col_map[0x8000];

void init_pix_col_map() {
	/* precalc colors so we get some fps */
	int a,b,c,A,B,C,rA,rB,rC;
	if (has_pix_col_map) return;
	for (a=0; a<0x20; a++) {
		A=a<<10; rA=A<<9;
		for (b=0; b<0x20; b++) {
			B=b<<5; rB=B<<6;
			for (c=0; c<0x20; c++) {
				C=c; rC=C<<3;
				pix_col_map[A|B|C]=rA|rB|rC;
			}
		}
	}
	has_pix_col_map=1;
}

#define RAW_W 256
#define RAW_H 192*2
#define MAX_SIZE 3
u32 on_screen_image32[RAW_W*RAW_H*MAX_SIZE*MAX_SIZE];

int inline screen_size() {
	return RAW_W*RAW_H*ScreenCoeff_Size*ScreenCoeff_Size*sizeof(u32);
}
int inline offset_pixels_lower_screen() {
	return screen_size()/2;
}

void black_screen () {
	/* removes artifacts when resizing with scanlines */
	memset(on_screen_image32,0,screen_size());
}

void decode_screen () {

	int x,y, m, W,H,L,BL;
	u32 image[RAW_H][RAW_W], pix;
	u16 * pixel = (u16*)&GPU_screen;
	u32 * rgb32 = &on_screen_image32[0];

	/* decode colors */
	init_pix_col_map();
	for (y=0; y<RAW_H; y++) {
		for (x=0; x<RAW_W; x++) {
			image[y][x] = pix_col_map[*pixel&0x07FFF];
			pixel++;
		}
	}
#define LOOP(a,b,c,d,e,f) \
		L=W*ScreenCoeff_Size; \
		BL=L*sizeof(u32); \
		for (a; b; c) { \
			for (d; e; f) { \
				pix = image[y][x]; \
				for (m=0; m<ScreenCoeff_Size; m++) { \
					*rgb32 = pix; rgb32++; \
				} \
			} \
			/* lines duplicated for scaling height */ \
			for (m=1; m<ScreenCoeff_Size; m++) { \
				memmove(rgb32, rgb32-L, BL); \
				rgb32 += L; \
			} \
		}

	/* load pixels in buffer accordingly */
	if (ScreenRotate) {
		W=RAW_H/2; H=RAW_W;
		LOOP(x=RAW_W-1, x >= 0, x--, y=0, y < W, y++)
		LOOP(x=RAW_W-1, x >= 0, x--, y=W, y < RAW_H, y++)
	} else {
		H=RAW_H; W=RAW_W;
		LOOP(y=0, y < RAW_H, y++, x=0, x < RAW_W, x++)

	}
}

int screen (GtkWidget * widget, int offset_pix) {
	int H,W,L;
	if (ScreenRotate) {
		W=RAW_H/2; H=RAW_W;
	} else {
		H=RAW_H/2; W=RAW_W;
	}
	L=W*ScreenCoeff_Size*sizeof(u32);

	gdk_draw_rgb_32_image	(widget->window,
		widget->style->fg_gc[widget->state],0,0, 
		W*ScreenCoeff_Size, H*ScreenCoeff_Size,
		GDK_RGB_DITHER_NONE,((guchar*)on_screen_image32)+offset_pix,L);
	
	return 1;
}

/* OUTPUT UPPER SCREEN  */
void      on_wDraw_Main_realize       (GtkWidget *widget, gpointer user_data) { }
gboolean  on_wDraw_Main_expose_event  (GtkWidget *widget, GdkEventExpose  *event, gpointer user_data) {
	decode_screen();
	return screen(widget, 0);
}

/* OUTPUT LOWER SCREEN  */
void      on_wDraw_Sub_realize        (GtkWidget *widget, gpointer user_data) { }
gboolean  on_wDraw_Sub_expose_event   (GtkWidget *widget, GdkEventExpose  *event, gpointer user_data) {
	return screen(widget, offset_pixels_lower_screen());
}







/* ***** ***** INPUT STYLUS / MOUSE ***** ***** */

void set_touch_pos (int x, int y) {
	s32 EmuX, EmuY;
	x /= ScreenCoeff_Size;
	y /= ScreenCoeff_Size;
	EmuX = x; EmuY = y;
	if (ScreenRotate) { EmuX = 256-y; EmuY = x; }
	if(EmuX<0) EmuX = 0; else if(EmuX>255) EmuX = 255;
	if(EmuY<0) EmuY = 0; else if(EmuY>192) EmuY = 192;
	NDS_setTouchPos(EmuX, EmuY);
}

gboolean  on_wDraw_Sub_button_press_event   (GtkWidget *widget, GdkEventButton  *event, gpointer user_data) {
	GdkModifierType state;
	gint x,y;
	
	if(desmume_running()) 
	if(event->button == 1) 
	{
		click = TRUE;	
		gdk_window_get_pointer(widget->window, &x, &y, &state);
		if (state & GDK_BUTTON1_MASK)
			set_touch_pos(x,y);
	}	
	return TRUE;
}

gboolean  on_wDraw_Sub_button_release_event (GtkWidget *widget, GdkEventButton  *event, gpointer user_data) {
	if(click) NDS_releasTouch();
	click = FALSE;
	return TRUE;
}

gboolean  on_wDraw_Sub_motion_notify_event  (GtkWidget *widget, GdkEventMotion  *event, gpointer user_data) {
	GdkModifierType state;
	gint x,y;
	
	if(click)
	{
		if(event->is_hint)
			gdk_window_get_pointer(widget->window, &x, &y, &state);
		else
		{
			x= (gint)event->x;
			y= (gint)event->y;
			state=(GdkModifierType)event->state;
		}
		
	//	fprintf(stderr,"X=%d, Y=%d, S&1=%d\n", x,y,state&GDK_BUTTON1_MASK);
	
		if(state & GDK_BUTTON1_MASK)
			set_touch_pos(x,y);
	}
	
	return TRUE;
}




/* ***** ***** KEYBOARD CONFIG / KEY DEFINITION ***** ***** */
u16 Keypad_Temp[NB_KEYS];
guint temp_Key=0;

void init_labels() {
	int i;
	char text[50], bname[20];
	GtkButton *b;
	for (i=0; i<NB_KEYS; i++) {
		sprintf(text,"%s : %s\0\0",key_names[i],KEYNAME(keyboard_cfg[i]));
		sprintf(bname,"button_%s\0\0",key_names[i]);
		b = (GtkButton*)glade_xml_get_widget(xml, bname);
		gtk_button_set_label(b,text);
	}
}

/* Initialize the joystick controls labels for the configuration window */
void init_joy_labels() {
  int i;
  char text[50], bname[30];
  GtkButton *b;
  for (i=0; i<4; i++) {
    /* Key not configured */
    if( joypad_cfg[i] == (u16)(-1) ) continue;

    sprintf(text,"%s : %d\0\0",key_names[i],joypad_cfg[i]);
    sprintf(bname,"button_joy_%s\0\0",key_names[i]);
    b = (GtkButton*)glade_xml_get_widget(xml, bname);
    gtk_button_set_label(b,text);
  }
  /* Skipping Axis */
  for (i=8; i<NB_KEYS; i++) {
    /* Key not configured */
    if( joypad_cfg[i] == (u16)(-1) ) continue;

    sprintf(text,"%s : %d\0\0",key_names[i],joypad_cfg[i]);
    sprintf(bname,"button_joy_%s\0\0",key_names[i]);
    b = (GtkButton*)glade_xml_get_widget(xml, bname);
    gtk_button_set_label(b,text);
  }
}

void edit_controls() {
	GtkDialog * dlg = (GtkDialog*)glade_xml_get_widget(xml, "wKeybConfDlg");
	memcpy(&Keypad_Temp, &keyboard_cfg, sizeof(keyboard_cfg));
	/* we change the labels so we know keyb def */
	init_labels();
	gtk_widget_show((GtkWidget*)dlg);
}

void  on_wKeybConfDlg_response (GtkDialog *dialog, gint arg1, gpointer user_data) {
	/* overwrite keyb def if user selected ok */
	if (arg1 == GTK_RESPONSE_OK)
		memcpy(&keyboard_cfg, &Keypad_Temp, sizeof(keyboard_cfg));
	gtk_widget_hide((GtkWidget*)dialog);
}

void inline current_key_label() {
	GtkLabel * lbl = (GtkLabel*)glade_xml_get_widget(xml, "label_key");
	gtk_label_set_text(lbl, KEYNAME(temp_Key));
}

gboolean  on_wKeyDlg_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	temp_Key = event->keyval;
	current_key_label();
	return TRUE;
}

void ask(GtkButton*b, int key) {
	char text[50];
	GtkDialog * dlg = (GtkDialog*)glade_xml_get_widget(xml, "wKeyDlg");
	key--; /* key = bit position, start with 1 */
	temp_Key = Keypad_Temp[key];
	current_key_label();
	switch (gtk_dialog_run(dlg))
	{
		case GTK_RESPONSE_OK:
			Keypad_Temp[key]=temp_Key;
			sprintf(text,"%s : %s\0\0",key_names[key],KEYNAME(temp_Key));
			gtk_button_set_label(b,text);
			break;
		case GTK_RESPONSE_CANCEL:
		case GTK_RESPONSE_NONE:
			break;
	}
	gtk_widget_hide((GtkWidget*)dlg);
}

void  on_button_Left_clicked    (GtkButton *b, gpointer user_data) { ask(b,KEY_LEFT); }
void  on_button_Up_clicked      (GtkButton *b, gpointer user_data) { ask(b,KEY_UP); }
void  on_button_Right_clicked   (GtkButton *b, gpointer user_data) { ask(b,KEY_RIGHT); }
void  on_button_Down_clicked    (GtkButton *b, gpointer user_data) { ask(b,KEY_DOWN); }

void  on_button_L_clicked       (GtkButton *b, gpointer user_data) { ask(b,KEY_L); }
void  on_button_R_clicked       (GtkButton *b, gpointer user_data) { ask(b,KEY_R); }

void  on_button_Y_clicked       (GtkButton *b, gpointer user_data) { ask(b,KEY_Y); }
void  on_button_X_clicked       (GtkButton *b, gpointer user_data) { ask(b,KEY_X); }
void  on_button_A_clicked       (GtkButton *b, gpointer user_data) { ask(b,KEY_A); }
void  on_button_B_clicked       (GtkButton *b, gpointer user_data) { ask(b,KEY_B); }

void  on_button_Start_clicked   (GtkButton *b, gpointer user_data) { ask(b,KEY_START); }
void  on_button_Select_clicked  (GtkButton *b, gpointer user_data) { ask(b,KEY_SELECT); }
void  on_button_Debug_clicked   (GtkButton *b, gpointer user_data) { ask(b,KEY_DEBUG); }
void  on_button_Boost_clicked   (GtkButton *b, gpointer user_data) { ask(b,KEY_BOOST); }

/* Joystick configuration / Key definition */
void ask_joy_key(GtkButton*b, int key)
{
  char text[50];
  u16 joykey;

  key--; /* remove 1 to get index */
  GtkDialog * dlg = (GtkDialog*)glade_xml_get_widget(xml, "wJoyDlg");
  gtk_widget_show_now(dlg);
  /* Need to force event processing. Otherwise, popup won't show up. */
  while ( gtk_events_pending() ) gtk_main_iteration();
  joykey = get_set_joy_key(key);
  sprintf(text,"%s : %d\0\0",key_names[key],joykey);
  gtk_button_set_label(b,text);
  gtk_widget_hide((GtkWidget*)dlg);
}

void on_joy_button_A_clicked (GtkButton *b, gpointer user_data)
{ ask_joy_key(b,KEY_A); }
void on_joy_button_B_clicked (GtkButton *b, gpointer user_data)
{ ask_joy_key(b,KEY_B); }
void on_joy_button_X_clicked (GtkButton *b, gpointer user_data)
{ ask_joy_key(b,KEY_X); }
void on_joy_button_Y_clicked (GtkButton *b, gpointer user_data)
{ ask_joy_key(b,KEY_Y); }
void on_joy_button_L_clicked (GtkButton *b, gpointer user_data)
{ ask_joy_key(b,KEY_L); }
void on_joy_button_R_clicked (GtkButton *b, gpointer user_data)
{ ask_joy_key(b,KEY_R); }
void on_joy_button_Select_clicked (GtkButton *b, gpointer user_data)
{ ask_joy_key(b,KEY_SELECT); }
void on_joy_button_Start_clicked (GtkButton *b, gpointer user_data)
{ ask_joy_key(b,KEY_START); }
void on_joy_button_Boost_clicked (GtkButton *b, gpointer user_data)
{ ask_joy_key(b,KEY_BOOST); }
void on_joy_button_Debug_clicked (GtkButton *b, gpointer user_data)
{ ask_joy_key(b,KEY_DEBUG); }
