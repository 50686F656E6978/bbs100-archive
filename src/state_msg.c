/*
    bbs100 1.2.2 WJ103
    Copyright (C) 2003  Walter de Jong <walter@heiho.net>

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*
	state_msg.c	WJ99
*/

#include "config.h"
#include "defines.h"
#include "state_msg.h"
#include "state.h"
#include "edit.h"
#include "access.h"
#include "util.h"
#include "log.h"
#include "debug.h"
#include "Stats.h"
#include "screens.h"
#include "CachedFile.h"
#include "strtoul.h"
#include "Param.h"
#include "OnlineUser.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>


void enter_message(User *usr) {
	Enter(enter_message);

	if (usr->curr_room == usr->mail)
		Print(usr, "<white>%sail message\n<green>",
			(usr->runtime_flags & RTF_UPLOAD) ? "Upload m" : "M");
	else
		Print(usr, "<white>%s message\n<green>",
			(usr->runtime_flags & RTF_UPLOAD) ? "Upload" : "Enter");

	if (usr->curr_room->flags & ROOM_CHATROOM) {
		Put(usr, "<red>You cannot enter a message in a <white>chat<red> room\n");
		CURRENT_STATE(usr);
		Return;
	}
	if (is_guest(usr->name)) {
		Print(usr, "<red>Sorry, but the <yellow>%s<red> user cannot enter messages\n", PARAM_NAME_GUEST);
		CURRENT_STATE(usr);
		Return;
	}
	if ((usr->curr_room->flags & ROOM_READONLY) && !(usr->runtime_flags & (RTF_SYSOP | RTF_ROOMAIDE))) {
		Put(usr, "<red>You are not allowed to post in this room\n");
		CURRENT_STATE(usr);
		Return;
	}
	destroy_Message(usr->new_message);
	if ((usr->new_message = new_Message()) == NULL) {
		Perror(usr, "Out of memory");
		CURRENT_STATE(usr);
		Return;
	}
	strcpy(usr->new_message->from, usr->name);

	if (usr->runtime_flags & RTF_SYSOP)
		usr->new_message->flags |= MSG_FROM_SYSOP;
	else
		if (usr->runtime_flags & RTF_ROOMAIDE)
			usr->new_message->flags |= MSG_FROM_ROOMAIDE;

	if (usr->curr_room == usr->mail)
		enter_recipients(usr, STATE_ENTER_MAIL_RECIPIENTS);
	else {
		if (usr->curr_room->flags & ROOM_ANONYMOUS)
			CALL(usr, STATE_POST_AS_ANON);
		else {
			if (usr->curr_room->flags & ROOM_SUBJECTS)
				CALL(usr, STATE_ENTER_SUBJECT);
			else
				enter_the_message(usr);
		}
	}
	Return;
}

void state_post_as_anon(User *usr, char c) {
	if (usr == NULL)
		return;

	Enter(state_post_as_anon);

	switch(c) {
		case INIT_STATE:
			usr->runtime_flags |= RTF_BUSY;
			break;

		case KEY_ESC:
		case KEY_CTRL('C'):
		case KEY_CTRL('D'):
			RET(usr);
			Return;

		case 'n':
		case 'N':
		case KEY_RETURN:
		case ' ':
			Put(usr, "<white>Normal\n");
			usr->runtime_flags &= ~(RTF_ANON | RTF_DEFAULT_ANON);

			if (usr->curr_room->flags & ROOM_SUBJECTS)
				JMP(usr, STATE_ENTER_SUBJECT);
			else {
				POP(usr);
				enter_the_message(usr);
			}
			Return;

		case 'a':
		case 'A':
			Put(usr, "<white>Anonymous\n");
			usr->runtime_flags |= RTF_ANON;
			JMP(usr, STATE_ENTER_ANONYMOUS);
			Return;

		case 'd':
		case 'D':
			Put(usr, "<white>Default-anonymous\n");
			usr->runtime_flags |= RTF_DEFAULT_ANON;

			if (usr->default_anon != NULL && usr->default_anon[0]) {
				cstrlwr(usr->default_anon);
				strcpy(usr->new_message->anon, usr->default_anon);
			} else
				strcpy(usr->new_message->anon, "anonymous");

			if (usr->curr_room->flags & ROOM_SUBJECTS)
				JMP(usr, STATE_ENTER_SUBJECT);
			else {
				POP(usr);
				enter_the_message(usr);
			}
			Return;
	}
	Put(usr, "\n<green>You can post as: <hotkey>Normal, <hotkey>Anonymous, <hotkey>Default-anonymous: ");
	Return;
}

void state_enter_anonymous(User *usr, char c) {
int r;

	if (usr == NULL)
		return;

	Enter(state_enter_anonymous);

	if (usr->new_message == NULL) {
		Perror(usr, "The message you were going to enter has disappeared");
		RET(usr);
		Return;
	}
	if (c == INIT_STATE)
		Put(usr, "<green>Enter alias<yellow>: <cyan>");

	r = edit_name(usr, c);
	if (r == EDIT_BREAK) {
		RET(usr);
		Return;
	}
	if (r == EDIT_RETURN) {
		if (!usr->edit_buf[0])
			strcpy(usr->new_message->anon, "anonymous");
		else {
			strcpy(usr->new_message->anon, usr->edit_buf);
			cstrlwr(usr->new_message->anon);
		}
		if (usr->curr_room->flags & ROOM_SUBJECTS)
			JMP(usr, STATE_ENTER_SUBJECT);
		else {
			POP(usr);
			enter_the_message(usr);
		}
	}
	Return;
}


void state_enter_mail_recipients(User *usr, char c) {
	if (usr == NULL)
		return;

	Enter(state_enter_mail_recipients);

	switch(edit_recipients(usr, c, multi_mail_access)) {
		case EDIT_BREAK:
			RET(usr);
			break;

		case EDIT_RETURN:
			if (usr->recipients == NULL) {
				RET(usr);
				break;
			}
			if (usr->new_message == NULL) {
				Perror(usr, "Your letter has caught fire or something");
				RET(usr);
				break;
			}
			if ((usr->new_message->to = copy_StringList(usr->recipients)) == NULL) {
				Perror(usr, "Out of memory");
				RET(usr);
				break;
			}
			listdestroy_StringList(usr->recipients);
			usr->recipients = NULL;

			JMP(usr, STATE_ENTER_SUBJECT);
	}
	Return;
}

void state_enter_subject(User *usr, char c) {
int r;

	if (usr == NULL)
		return;

	Enter(state_enter_subject);

	if (c == INIT_STATE)
		Put(usr, "<green>Subject: <yellow>");

	r = edit_line(usr, c);

	if (r == EDIT_BREAK) {
		RET(usr);
		Return;
	}
	if (r == EDIT_RETURN) {
		if (usr->edit_buf[0])
			strcpy(usr->new_message->subject, usr->edit_buf);

		POP(usr);				/* pop this 'enter subject' call */
		enter_the_message(usr);
	}
	Return;
}

void enter_the_message(User *usr) {
	if (usr == NULL)
		return;

	Enter(enter_the_message);

	usr->new_message->mtime = rtc;
	if (usr->new_message->anon[0])
		usr->new_message->flags &= ~(MSG_FROM_SYSOP | MSG_FROM_ROOMAIDE);

	if (usr->runtime_flags & RTF_UPLOAD)
		Print(usr, "<green>Upload %smessage, press <white><<yellow>Ctrl-C<white>><green> to end\n",
			(usr->curr_room == usr->mail) ? "mail " : "");
	else
		Print(usr, "<green>Enter %smessage, press <white><<yellow>return<white>><green> twice or press <white><<yellow>Ctrl-C<white>><green> to end\n",
			(usr->curr_room == usr->mail) ? "mail " : "");
	print_new_msg_header(usr);
	edit_text(usr, save_message, abort_message);
	Return;
}

void save_message(User *usr, char c) {
char filename[MAX_PATHLEN], *p;
int err = 0;
Joined *j;
StringList *sl;

	if (usr == NULL)
		return;

	Enter(save_message);

	if (usr->new_message == NULL) {
		Perror(usr, "The message has disappeared!");
		RET(usr);
		Return;
	}
	listdestroy_StringList(usr->new_message->msg);
	usr->new_message->msg = rewind_StringList(usr->more_text);
	usr->more_text = NULL;

	if (usr->curr_room == NULL) {
		Print(usr, "<red>In the meantime, the current room has vanished\n"
			"You are dropped off in the <yellow>%s<white>>\n", Lobby_room->name);

		destroy_Message(usr->new_message);
		usr->new_message = NULL;

		POP(usr);
		goto_room(usr, Lobby_room);
		Return;
	}
/*
	check for empty messages (completely empty or only containing spaces)
*/
	for(sl = usr->new_message->msg; sl != NULL; sl = sl->next) {
		for(p = sl->str; *p; p++) {
			if (*p != ' ')
				break;
		}
		if (*p)
			break;
	}
	if (sl == NULL) {
		Print(usr, "<red>Message is empty, message not saved\n");

		destroy_Message(usr->new_message);
		usr->new_message = NULL;
		RET(usr);
		Return;
	}
	if (usr->curr_room == usr->mail) {
		StringList *sl, *sl_next;
		User *u;

		for(sl = usr->new_message->to; sl != NULL; sl = sl_next) {
			sl_next = sl->next;

			if (!strcmp(usr->name, sl->str))
				continue;

			if ((u = is_online(sl->str)) == NULL) {
				if (!(usr->runtime_flags & RTF_SYSOP)) {	/* if not sysop, check permissions */
					StringList *sl2;

					if ((u = new_User()) == NULL) {
						Perror(usr, "Out of memory");
						RET(usr);
						Return;
					}
					if (load_User(u, sl->str, LOAD_USER_ENEMYLIST)) {
						Perror(usr, "Failed to load user");
						destroy_User(u);
						u = NULL;
						continue;
					}
					if ((sl2 = in_StringList(u->enemies, usr->name)) != NULL) {
						Print(usr, "<yellow>%s<red> does not wish to receive <yellow>Mail<white>><red> from you\n", sl->str);

						remove_StringList(&usr->new_message->to, sl2);
						destroy_StringList(sl2);
						destroy_User(u);
						u = NULL;
						continue;
					}
					destroy_User(u);
					u = NULL;
				}
				usr->new_message->number = get_mail_top(sl->str)+1;
			} else
				usr->new_message->number = room_top(u->mail)+1;

			sprintf(filename, "%s/%c/%s/%lu", PARAM_USERDIR, sl->str[0], sl->str, usr->new_message->number);
			path_strip(filename);

			if (!save_Message(usr->new_message, filename)) {
				Print(usr, "<green>Sending mail to <yellow>%s\n", sl->str);

				if (u != NULL) {
					newMsg(u->mail, usr->new_message);
					Tell(u, "<beep><cyan>You have new mail\n");
				}
			} else {
				Print(usr, "<red>Error delivering mail to <yellow>%s\n", sl->str);
				err++;
			}
		}
/* save a copy for the sender */

		if (usr->new_message->to != NULL) {
			if (!err) {
				usr->new_message->number = room_top(usr->mail)+1;
				sprintf(filename, "%s/%c/%s/%lu", PARAM_USERDIR, usr->name[0], usr->name, usr->new_message->number);
				path_strip(filename);

				if (save_Message(usr->new_message, filename))
					err++;
				else
					newMsg(usr->mail, usr->new_message);
			}
			if (err) {
				Perror(usr, "Error sending mail");
			}
		}
	} else {
		if ((usr->curr_room->flags & ROOM_READONLY) && !(usr->runtime_flags & (RTF_SYSOP | RTF_ROOMAIDE))) {
			Put(usr, "<red>You are suddenly not allowed to post in this room\n");
			destroy_Message(usr->new_message);
			usr->new_message = NULL;
			RET(usr);
			Return;
		}
		if (!(usr->runtime_flags & RTF_SYSOP) && in_StringList(usr->curr_room->kicked, usr->name) != NULL) {
			Put(usr, "<red>In the meantime, you have been kicked from this room\n");
			if ((j = in_Joined(usr->rooms, usr->curr_room->number)) != NULL)
				j->zapped = 1;

			destroy_Message(usr->new_message);
			usr->new_message = NULL;
			POP(usr);
			goto_room(usr, Lobby_room);
			Return;
		}
		usr->new_message->number = room_top(usr->curr_room)+1;
		sprintf(filename, "%s/%u/%lu", PARAM_ROOMDIR, usr->curr_room->number, usr->new_message->number);
		path_strip(filename);

		if (!save_Message(usr->new_message, filename)) {
			newMsg(usr->curr_room, usr->new_message);
			room_beep(usr, usr->curr_room);
			expire_msg(usr->curr_room);			/* expire old messages */
		} else {
			err++;
			Perror(usr, "Error saving message");
		}
	}
/* update stats */
	if (!err && !(usr->new_message->to != NULL && usr->new_message->to->next == NULL
		&& !strcmp(usr->name, usr->new_message->to->str))) {
		usr->posted++;
		if (usr->posted > stats.posted) {
			stats.posted = usr->posted;
			strcpy(stats.most_posted, usr->name);
		}
	}
	RET(usr);
	Return;
}

void abort_message(User *usr, char c) {
	if (usr == NULL)
		return;

	Enter(abort_message);

	listdestroy_StringList(usr->more_text);
	usr->more_text = NULL;

	RET(usr);
	Return;
}


void edit_text(User *usr, void (*save)(User *, char), void (*abort)(User *, char)) {
	Enter(edit_text);

	PUSH(usr, save);
	PUSH(usr, abort);
	CALL(usr, STATE_EDIT_TEXT);

	Return;
}

void state_edit_text(User *usr, char c) {
int r;

	if (usr == NULL)
		return;

	Enter(state_edit_text);

	if (c == INIT_STATE) {
		listdestroy_StringList(usr->more_text);
		usr->more_text = usr->textp = NULL;
		usr->total_lines = 0;

		if (usr->curr_room == usr->mail)
			usr->runtime_flags |= RTF_BUSY_MAILING;
	}
	r = edit_msg(usr, c);

	if (r == EDIT_RETURN) {
		if (!usr->edit_buf[0]) {
			if (usr->runtime_flags & RTF_UPLOAD) {
				usr->edit_buf[0] = ' ';
				usr->edit_buf[1] = 0;
			} else
				r = EDIT_BREAK;
		}
	}
	if (r == EDIT_RETURN || r == EDIT_BREAK) {
		if (usr->edit_buf[0]) {
			usr->more_text = add_StringList(&usr->more_text, new_StringList(usr->edit_buf));
			usr->total_lines++;
		}
		usr->edit_pos = 0;
		usr->edit_buf[0] = 0;
	}
	if (r == EDIT_BREAK) {
		JMP(usr, STATE_SAVE_TEXT);
	}
	Return;
}

void state_save_text(User *usr, char c) {
	if (usr == NULL)
		return;

	Enter(state_save_text);

	switch(c) {
		case INIT_STATE:
			Put(usr, "<hotkey>A<green>bort, <hotkey>Save, <hotkey>Continue<white>: ");
			usr->runtime_flags |= RTF_BUSY;
			break;

		case 'a':
		case 'A':
		case KEY_CTRL('C'):
		case KEY_CTRL('D'):
			Put(usr, "<white>Abort -- ");
			JMP(usr, STATE_ABORT_TEXT);
			break;

		case 'c':
		case 'C':
			Put(usr, "<white>Continue<green>\n");

			if (usr->total_lines >= PARAM_MAX_MSG_LINES) {
				Put(usr, "<red>You already have entered too many lines\n\n");
				CURRENT_STATE(usr);
			} else
				MOV(usr, STATE_EDIT_TEXT);
			break;

		case 's':
		case 'S':
			Put(usr, "<white>Save\n");
			usr->runtime_flags &= ~RTF_BUSY_MAILING;
			POP(usr);				/* pop the abort_text() user exit */
			RET(usr);
			break;

		default:
			Put(usr, "\n");
			CURRENT_STATE(usr);
	}
	Return;
}

void state_abort_text(User *usr, char c) {
void (*abort_func)(User *, char);

	if (usr == NULL)
		return;

	Enter(state_abort_text);

	if (c == INIT_STATE) {
		Put(usr, "<cyan>Are you sure? <white>(<cyan>y<white>/<cyan>N<white>): ");
		Return;
	}
	switch(yesno(usr, c, 'N')) {
		case YESNO_YES:
			listdestroy_StringList(usr->more_text);
			usr->more_text = NULL;

			usr->runtime_flags &= ~RTF_BUSY_MAILING;
/*
	fiddle with stack ; remove the abort() and save() handlers, and jump to
	the abort() user exit function
*/
			POP(usr);							/* pop current */
			abort_func = usr->callstack->ip;	/* abort func */
			POP(usr);							/* pop abort func */
			JMP(usr, abort_func);				/* overwrite save() address */
			break;

		case YESNO_NO:
			JMP(usr, STATE_SAVE_TEXT);
			break;

		case YESNO_UNDEF:
			CURRENT_STATE(usr);
			break;
	}
	Return;
}


void state_enter_msg_number(User *usr, char c) {
int r;

	if (usr == NULL)
		return;

	Enter(state_enter_msg_number);

	if (c == INIT_STATE)
		Put(usr, "<green>Enter message number<yellow>: ");

	r = edit_number(usr, c);

	if (r == EDIT_BREAK) {
		RET(usr);
		Return;
	}
	if (r == EDIT_RETURN) {
		unsigned long num;
		MsgIndex *idx;

		num = strtoul(usr->edit_buf, NULL, 10);
		if (num <= 0) {
			Put(usr, "<red>No such message\n");
			RET(usr);
			Return;
		}
		for(idx = usr->curr_room->msgs; idx != NULL; idx = idx->next) {
			if (idx->number == num)
				break;

			if (idx->number > num) {	/* we're not getting there anymore */
				idx = NULL;
				break;
			}
		}
		if (idx == NULL) {
			Put(usr, "<red>No such message\n");
			RET(usr);
			Return;
		}
		usr->curr_msg = idx;
		readMsg(usr);
	}
	Return;
}

void state_enter_minus_msg(User *usr, char c) {
int r;

	if (usr == NULL)
		return;

	Enter(state_enter_minus_msg);

	if (c == INIT_STATE)
		Put(usr, "<green>Enter number of messages to read back<yellow>: ");

	r = edit_number(usr, c);

	if (r == EDIT_BREAK) {
		RET(usr);
		Return;
	}
	if (r == EDIT_RETURN) {
		int i;

		i = atoi(usr->edit_buf);
		if (i <= 0) {
			RET(usr);
			Return;
		}
		if (usr->curr_msg == NULL) {
			if (usr->curr_room->msgs == NULL) {
				Put(usr, "<red>No messages\n");
				RET(usr);
				Return;
			}
			usr->curr_msg = unwind_MsgIndex(usr->curr_room->msgs);
		}
		while(i > 1) {
			if (usr->curr_msg->prev != NULL)
				usr->curr_msg = usr->curr_msg->prev;
			else
				break;
			i--;
		}
		readMsg(usr);			/* readMsg() RET()s for us :P */
	}
	Return;
}

void state_enter_plus_msg(User *usr, char c) {
int r;

	if (usr == NULL)
		return;

	Enter(state_enter_plus_msg);

	if (c == INIT_STATE)
		Put(usr, "<green>Enter number of messages to skip<yellow>: ");

	r = edit_number(usr, c);

	if (r == EDIT_BREAK) {
		RET(usr);
		Return;
	}
	if (r == EDIT_RETURN) {
		int i;

		i = atoi(usr->edit_buf);
		if (i <= 0) {
			RET(usr);
			Return;
		}
		if (usr->curr_msg == NULL) {
			if (usr->curr_room->msgs == NULL) {
				Put(usr, "<red>No messages\n");
				RET(usr);
				Return;
			}
			usr->curr_msg = rewind_MsgIndex(usr->curr_room->msgs);
		}
		while(i > 0) {
			if (usr->curr_msg->next != NULL)
				usr->curr_msg = usr->curr_msg->next;
			else
				break;
			i--;
		}
		readMsg(usr);			/* readMsg() RET()s for us :P */
	}
	Return;
}

/* --- */


void readMsg(User *usr) {
char filename[MAX_PATHLEN];
Joined *j;

	if (usr == NULL)
		return;

	Enter(readMsg);

	if (usr->curr_msg == NULL) {
		Perror(usr, "The message you were attempting to read has all of a sudden vaporized");
		RET(usr);
		Return;
	}
	if ((j = in_Joined(usr->rooms, usr->curr_room->number)) == NULL) {
		Perror(usr, "Suddenly you haven't joined this room (??)");
		RET(usr);
		Return;
	}
	if (usr->curr_msg->number > j->last_read)
		j->last_read = usr->curr_msg->number;

/* construct filename */
	if (usr->curr_room == usr->mail)
		sprintf(filename, "%s/%c/%s/%lu", PARAM_USERDIR, usr->name[0], usr->name, usr->curr_msg->number);
	else
		sprintf(filename, "%s/%u/%lu", PARAM_ROOMDIR, usr->curr_room->number, usr->curr_msg->number);
	path_strip(filename);

/* load the message */
	destroy_Message(usr->message);
	if ((usr->message = load_Message(filename, usr->curr_msg)) == NULL) {
		Perror(usr, "The message vaporizes as you attempt to read it");
		RET(usr);
		Return;
	}
	more_msg_header(usr);

	if (usr->message->deleted != (time_t)0UL) {
		usr->more_text = add_StringList(&usr->more_text, new_StringList(""));

		if (usr->message->flags & MSG_DELETED_BY_ANON)
			usr->more_text = add_String(&usr->more_text, "<yellow>[<red>Deleted on <yellow>%s<red> by <cyan>- %s -<yellow>]",
				print_date(usr, usr->message->deleted), usr->message->anon);
		else {
			char deleted_by[MAX_LINE];

			deleted_by[0] = 0;
			if (usr->message->flags & MSG_DELETED_BY_SYSOP)
				strcpy(deleted_by, PARAM_NAME_SYSOP);
			else
				if (usr->message->flags & MSG_DELETED_BY_ROOMAIDE)
					strcpy(deleted_by, PARAM_NAME_ROOMAIDE);

			if (*deleted_by)
				strcat(deleted_by, ": ");
			strcat(deleted_by, usr->message->deleted_by);

			usr->more_text = add_String(&usr->more_text, "<yellow>[<red>Deleted on <yellow>%s<red> by <white>%s<yellow>]",
				print_date(usr, usr->message->deleted), deleted_by);
		}
	} else {
		StringList *sl;

		if (usr->curr_room->flags & ROOM_SUBJECTS) {		/* room has subject lines */
			if (usr->message->subject[0]) {
				if (usr->message->flags & MSG_FORWARDED)
					usr->more_text = add_String(&usr->more_text, "<cyan>Subject: <white>Fwd: <yellow>%s", usr->message->subject);
				else
					if (usr->message->flags & MSG_REPLY)
						usr->more_text = add_String(&usr->more_text, "<cyan>Subject: <white>Re: <yellow>%s", usr->message->subject);
					else
						usr->more_text = add_String(&usr->more_text, "<cyan>Subject: <yellow>%s", usr->message->subject);
			} else {
/*
	Note: in Mail>, the reply_number is always 0 so this message is never displayed there
	which is actually correct, because the reply_number would be different to each
	recipient of the Mail> message
*/
				if (usr->message->flags & MSG_REPLY && usr->message->reply_number)
					usr->more_text = add_String(&usr->more_text, "<cyan>Subject: <white>Re: <yellow><message #%lu>", usr->message->reply_number);
			}
		} else {
			if ((usr->message->flags & MSG_FORWARDED) && usr->message->subject[0])
				usr->more_text = add_String(&usr->more_text, "<white>Fwd: <yellow>%s", usr->message->subject);
			else {
				if (usr->message->flags & MSG_REPLY) {			/* room without subject lines */
					if (usr->message->subject[0])
						usr->more_text = add_String(&usr->more_text, "<white>Re: <yellow>%s", usr->message->subject);
					else
						if (usr->message->reply_number)
							usr->more_text = add_String(&usr->more_text, "<white>Re: <yellow><message #%lu>", usr->message->reply_number);
				}
			}
		}
		usr->more_text = add_StringList(&usr->more_text, new_StringList("<green>"));
		if (usr->message->msg != NULL) {
			if ((sl = copy_StringList(usr->message->msg)) == NULL) {
				listdestroy_StringList(usr->more_text);
				usr->more_text = NULL;

				Perror(usr, "Out of memory");
				RET(usr);
				Return;
			}
			concat_StringList(&usr->more_text, sl);
		}
		usr->read++;						/* update stats */
		if (usr->read > stats.read) {
			stats.read = usr->read;
			strcpy(stats.most_read, usr->name);
		}
	}
	usr->textp = usr->more_text = rewind_StringList(usr->more_text);
	usr->read_lines = 0;
	usr->total_lines = list_Count(usr->more_text);

	JMP(usr, STATE_MORE_PROMPT);
	Return;
}


void state_del_msg_prompt(User *usr, char c) {
int r;

	if (usr == NULL)
		return;

	Enter(state_del_msg_prompt);

	if (c == INIT_STATE) {
		Print(usr, "<cyan>Are you sure? <white>(<cyan>y<white>/<cyan>N<white>): ");
		usr->runtime_flags |= RTF_BUSY;
		Return;
	}
	r = yesno(usr, c, 'Y');
	if (r == YESNO_YES) {
		char buf[MAX_PATHLEN];

/* mark message as deleted */
		usr->message->deleted = rtc;
		strcpy(usr->message->deleted_by, usr->name);

		if (usr->runtime_flags & RTF_SYSOP)
			usr->message->flags |= MSG_DELETED_BY_SYSOP;
		else
			if (usr->runtime_flags & RTF_ROOMAIDE)
				usr->message->flags |= MSG_DELETED_BY_ROOMAIDE;
			else
				if (usr->message->anon[0])
					usr->message->flags |= MSG_DELETED_BY_ANON;

		if (usr->curr_room == usr->mail)
			sprintf(buf, "%s/%c/%s/%lu", PARAM_USERDIR, usr->name[0], usr->name, usr->message->number);
		else
			sprintf(buf, "%s/%u/%lu", PARAM_ROOMDIR, usr->curr_room->number, usr->message->number);
		path_strip(buf);

		if (save_Message(usr->message, buf)) {
			Perror(usr, "Failed to delete message");
		} else
			Put(usr, "<green>Message deleted\n\n");
	}
	if (r == YESNO_UNDEF) {
		CURRENT_STATE(usr);
		Return;
	}
	usr->runtime_flags &= ~RTF_BUSY;
	RET(usr);
	Return;
}

void undelete_msg(User *usr) {
char filename[MAX_PATHLEN];

	if (usr == NULL)
		return;

	Enter(undelete_msg);

	if (usr->message == NULL) {
		Put(usr, "<red>No message to undelete!\n");
		Return;
	}
	if (usr->message->deleted == (time_t)0UL) {
		Put(usr, "<red>Message has not been deleted, so you can't undelete it..!\n\n");
		Return;
	}
	if ((usr->curr_room != usr->mail
		&& ((usr->message->flags & MSG_DELETED_BY_SYSOP) && !(usr->runtime_flags & RTF_SYSOP)))
		|| ((usr->message->flags & MSG_DELETED_BY_ROOMAIDE) && !(usr->runtime_flags & (RTF_SYSOP | RTF_ROOMAIDE)))
		|| (strcmp(usr->message->deleted_by, usr->name) && !(usr->runtime_flags & (RTF_SYSOP | RTF_ROOMAIDE)))) {
		Put(usr, "<red>You are not allowed to undelete this message\n\n");
		Return;
	}
	usr->message->deleted_by[0] = 0;
	usr->message->deleted = (time_t)0UL;
	usr->message->flags &= ~(MSG_DELETED_BY_SYSOP | MSG_DELETED_BY_ROOMAIDE | MSG_DELETED_BY_ANON);

	if (usr->curr_room == usr->mail)
		sprintf(filename, "%s/%c/%s/%lu", PARAM_USERDIR, usr->name[0], usr->name, usr->message->number);
	else
		sprintf(filename, "%s/%u/%lu", PARAM_ROOMDIR, usr->curr_room->number, usr->message->number);
	path_strip(filename);

	if (save_Message(usr->message, filename)) {
		Perror(usr, "Failed to undelete message");
	} else
		Put(usr, "<green>Message undeleted\n");
	Return;
}


/*
	receive a message that was sent
	If busy, the message will be put in a buffer
*/
void recvMsg(User *usr, User *from, BufferedMsg *msg) {
BufferedMsg *new_msg;
char msg_type[MAX_LINE];
StringList *sl;

	if (usr == NULL || from == NULL || msg == NULL)
		return;

	Enter(recvMsg);

	if (usr == from) {			/* talking to yourself is a no-no, because of kludgy code :P */
		Return;
	}
	if (usr->runtime_flags & RTF_LOCKED) {
		Print(from, "<red>Sorry, but <yellow>%s<red> has suddenly locked the terminal\n", usr->name);
		goto Rcv_Remove_Recipient;
	}
	if (!(from->runtime_flags & RTF_SYSOP)) {
		if ((sl = in_StringList(usr->enemies, from->name)) != NULL) {
			Print(from, "<red>Sorry, but <yellow>%s<red> does not wish to receive any messages from you\n"
				"any longer\n", usr->name);
			goto Rcv_Remove_Recipient;
		}
		if ((usr->flags & USR_X_DISABLED)
			&& (in_StringList(usr->friends, from->name) == NULL)) {
			Print(from, "<red>Sorry, but <yellow>%s<red> has suddenly disabled message reception\n", usr->name);

Rcv_Remove_Recipient:
			if ((sl = in_StringList(from->recipients, usr->name)) != NULL) {
				remove_StringList(&from->recipients, sl);
				destroy_StringList(sl);
			}
			Return;
		}
	}
	if (msg->flags & BUFMSG_XMSG) {
		strcpy(msg_type, "eXpress message");

		if (from != usr) {
			usr->xrecv++;							/* update stats */
			if (usr->xrecv > stats.xrecv) {
				stats.xrecv = usr->xrecv;
				strcpy(stats.most_xrecv, usr->name);
			}
		}
	} else {
		if (msg->flags & BUFMSG_EMOTE) {
			strcpy(msg_type, "Emote");

			if (from != usr) {
				usr->erecv++;						/* update stats */
				if (usr->erecv > stats.erecv) {
					stats.erecv = usr->erecv;
					strcpy(stats.most_erecv, usr->name);
				}
			}
		} else {
			if (msg->flags & BUFMSG_FEELING) {
				strcpy(msg_type, "Feeling");

				if (from != usr) {
					usr->frecv++;					/* update stats */
					if (usr->frecv > stats.frecv) {
						stats.frecv = usr->frecv;
						strcpy(stats.most_frecv, usr->name);
					}
				}
			} else
				if (msg->flags & BUFMSG_QUESTION)
					strcpy(msg_type, "Question");
				else
					strcpy(msg_type, "Message");
		}
	}
	if (usr->runtime_flags & (RTF_BUSY|RTF_HOLD))
		msg->flags &= ~BUFMSG_SEEN;
	else
		msg->flags |= BUFMSG_SEEN;

	if ((new_msg = copy_BufferedMsg(msg)) == NULL) {
		Perror(from, "Out of memory");
		Print(from, "<red>%s was not received by <yellow>%s\n", msg_type, usr->name);
		Return;
	}
/*
	put the new copy of the message on the correct list
*/
	if (usr->runtime_flags & RTF_HOLD) {
		if (!(msg->flags & ~BUFMSG_SEEN)) {		/* one-shot message */
			if (usr->runtime_flags & RTF_BUSY)
				add_BufferedMsg(&usr->busy_msgs, new_msg);
			else
				add_BufferedMsg(&usr->history, new_msg);
		} else
			add_BufferedMsg(&usr->held_msgs, new_msg);
	} else
		if (usr->runtime_flags & RTF_BUSY)
			add_BufferedMsg(&usr->busy_msgs, new_msg);
		else
			add_BufferedMsg(&usr->history, new_msg);

	if (usr->history_p != usr->history && list_Count(usr->history) > PARAM_MAX_HISTORY) {
		BufferedMsg *m;

		m = usr->history;						/* remove the head */
		usr->history = usr->history->next;
		usr->history->prev = NULL;
		destroy_BufferedMsg(m);
	}
	if (usr->runtime_flags & (RTF_BUSY|RTF_HOLD)) {
		if ((usr->runtime_flags & RTF_BUSY_SENDING)
			&& in_StringList(usr->recipients, from->name) != NULL)
/*
	warn follow-up mode by Richard of MatrixBBS
*/
			Print(from, "<yellow>%s<green> is busy sending you a message%s\n", usr->name,
				(usr->flags & USR_FOLLOWUP) ? " in follow-up mode" : "");
		else
			if ((usr->runtime_flags & RTF_BUSY_MAILING)
				&& in_StringList(usr->recipients, from->name) != NULL)
				Print(from, "<yellow>%s<green> is busy mailing you a message\n", usr->name);
			else
				if (usr->runtime_flags & RTF_HOLD)
					Print(from, "<yellow>%s<green> has put messages on hold for a while\n", usr->name);
				else
					Print(from, "<yellow>%s<green> is busy and will receive your %s when done\n", usr->name, msg_type);
		Return;
	}
	Put(usr, "<beep>");									/* alarm beep */
	print_buffered_msg(usr, new_msg);
	Put(usr, "\n");

	Print(from, "<green>%s received by <yellow>%s\n", msg_type, usr->name);

	if (in_StringList(from->talked_to, usr->name) == NULL)
		add_StringList(&from->talked_to, new_StringList(usr->name));

/* auto-reply if FOLLOWUP or if it was a Question */

	if ((usr->flags & USR_FOLLOWUP)
		|| ((msg->flags & BUFMSG_QUESTION) && (usr->flags & USR_HELPING_HAND))) {
		listdestroy_StringList(usr->recipients);
		if ((usr->recipients = new_StringList(msg->from)) == NULL) {
			Perror(usr, "Out of memory");
			Return;
		}
		do_reply_x(usr, msg->flags);
	}
	Return;
}

void spew_BufferedMsg(User *usr) {
StringList *sl;
BufferedMsg *m;

	if (usr == NULL)
		return;

	Enter(spew_BufferedMsg);

	while(usr->busy_msgs != NULL) {
		m = usr->busy_msgs;

		usr->busy_msgs = usr->busy_msgs->next;
		if (usr->busy_msgs != NULL)
			usr->busy_msgs->prev = NULL;

		m->prev = m->next = NULL;

		if (!m->flags) {							/* one shot message */
			for(sl = m->msg; sl != NULL; sl = sl->next)
				Print(usr, "%s\n", sl->str);

			destroy_BufferedMsg(m);					/* one-shots are not remembered */
			continue;
		}
		add_BufferedMsg(&usr->history, m);			/* remember this message */

		Put(usr, "<beep>");
		print_buffered_msg(usr, m);
		Put(usr, "\n");

/* auto-reply is follow up or question */
		if (strcmp(m->from, usr->name)
			&& ((usr->flags & USR_FOLLOWUP)
			|| ((m->flags & BUFMSG_QUESTION) && (usr->flags & USR_HELPING_HAND)))) {
			if (is_online(m->from) != NULL) {
				listdestroy_StringList(usr->recipients);
				usr->recipients = NULL;

				if ((usr->recipients = new_StringList(m->from)) == NULL) {
					Perror(usr, "Out of memory");
					Return;
				}
				do_reply_x(usr, m->flags);
				Return;
			} else
				Print(usr, "<red>Sorry, but <yellow>%s<red> left before you could reply!\n", m->from);
		}
	}
	Return;
}

/*
	Produce an X message header
	Note: returns a static buffer
*/
char *buffered_msg_header(User *usr, BufferedMsg *msg) {
static char buf[PRINT_BUF];
struct tm *tm;
char frombuf[256] = "", namebuf[256] = "", multi[32] = "", msgtype[64] = "";
int from_me = 0;

	if (usr == NULL || msg == NULL)
		return NULL;

	Enter(buffered_msg_header);

	if (!msg->flags) {			/* one-shot message */
		*buf = 0;
		Return buf;
	}
	tm = user_time(usr, msg->mtime);
	if ((usr->flags & USR_12HRCLOCK) && tm->tm_hour > 12)
		tm->tm_hour -= 12;		/* use 12 hour clock, no 'military' time */

	if (!strcmp(msg->from, usr->name)) {
		from_me = 1;
		if (msg->to != NULL) {
			if (msg->to->next != NULL)
				strcpy(namebuf, "<many>");
			else {
				if (!strcmp(msg->to->str, usr->name))
					strcpy(namebuf, "yourself");
				else
					strcpy(namebuf, msg->to->str);
			}
		} else
			strcpy(namebuf, "<bug!>");
	}
	if (msg->flags & BUFMSG_SYSOP) {
		if (from_me)
			sprintf(frombuf, "as %s ", PARAM_NAME_SYSOP);
		else
			sprintf(frombuf, "%c%s<white>:%c %s", (msg->flags & BUFMSG_EMOTE) ? (char)color_by_name("cyan") : (char)color_by_name("yellow"),
				PARAM_NAME_SYSOP, (msg->flags & BUFMSG_EMOTE) ? (char)color_by_name("cyan") : (char)color_by_name("yellow"), msg->from);
	} else {
		if (!from_me)
			sprintf(frombuf, "%c%s", (msg->flags & BUFMSG_EMOTE) ? (char)color_by_name("cyan") : (char)color_by_name("yellow"), msg->from);
	}
	if (msg->to != NULL && msg->to->next != NULL)
		strcpy(multi, "Multi ");

/* the message type */

	if (msg->flags & BUFMSG_XMSG)
		strcpy(msgtype, "eXpress message");
	else
		if (msg->flags & BUFMSG_EMOTE)
			strcpy(msgtype, "Emote");
		else
			if (msg->flags & BUFMSG_FEELING)
				strcpy(msgtype, "Feeling");
			else
				if (msg->flags & BUFMSG_QUESTION)
					strcpy(msgtype, "Question");

	if ((msg->flags & BUFMSG_EMOTE) && !from_me) {
		sprintf(buf, "<yellow>%c<white>%d<yellow>:<white>%02d<yellow>%c %s <yellow>", (multi[0] == 0) ? '(' : '[',
			tm->tm_hour, tm->tm_min, (multi[0] == 0) ? ')' : ']', frombuf);
	} else {
		if (msg->flags & (BUFMSG_XMSG | BUFMSG_EMOTE | BUFMSG_FEELING | BUFMSG_QUESTION)) {
			if (from_me)
				sprintf(buf, "<blue>*** <cyan>You sent this %s%s to <yellow>%s<cyan> %sat <white>%02d<yellow>:<white>%02d <blue>***<yellow>\n", multi, msgtype, namebuf, frombuf, tm->tm_hour, tm->tm_min);
			else
				sprintf(buf, "<blue>*** <cyan>%s%s received from %s<cyan> at <white>%02d<yellow>:<white>%02d <blue>***<yellow>\n", multi, msgtype, frombuf, tm->tm_hour, tm->tm_min);
		}
	}
	Return buf;
}

void print_buffered_msg(User *usr, BufferedMsg *msg) {
StringList *sl;

	if (usr == NULL || msg == NULL)
		return;

	Enter(print_buffered_msg);

/*
	update talked-to list
	we do it this late because you haven't really talked to anyone
	until you see the message they sent you
	(think about users that are in hold message mode)
*/
	if (msg->from[0] && strcmp(usr->name, msg->from) && in_StringList(usr->talked_to, msg->from) == NULL)
		add_StringList(&usr->talked_to, new_StringList(msg->from));

	Print(usr, "\n%s", buffered_msg_header(usr, msg));
	for(sl = msg->msg; sl != NULL; sl = sl->next)
		Print(usr, "%s\n", sl->str);

	Return;
}

void state_history_prompt(User *usr, char c) {
StringList *sl;
char prompt[PRINT_BUF];

	if (usr == NULL)
		return;

	Enter(state_history_prompt);

	switch(c) {
		case INIT_STATE:
			usr->history_p = unwind_BufferedMsg(usr->history);

			while(usr->history_p != NULL) {
				if (usr->history_p->flags & (BUFMSG_XMSG | BUFMSG_EMOTE | BUFMSG_FEELING | BUFMSG_QUESTION))
					break;
				usr->history_p = usr->history_p->prev;
			}
			if (usr->history_p == NULL) {
				Put(usr, "<red>No messages have been sent yet\n");
				RET(usr);
				Return;
			}
			print_buffered_msg(usr, usr->history_p);

			usr->runtime_flags |= RTF_BUSY;
			break;

		case 'b':
		case 'B':
		case 'p':
		case 'P':
		case KEY_BS:
		case '-':
		case '_':
			if (c == 'p' || c == 'P')
				Put(usr, "Previous\n");
			else
				Put(usr, "Back\n");

			if (usr->history_p == NULL) {
				Perror(usr, "Your history buffer is gone");
				usr->runtime_flags &= ~RTF_BUSY;
				RET(usr);
				Return;
			}
			if (usr->history_p->prev == NULL) {
				Put(usr, "<red>Can't go further back\n");
				break;
			}
			usr->history_p = usr->history_p->prev;
			print_buffered_msg(usr, usr->history_p);
			break;

		case 'f':
		case 'F':
		case 'n':
		case 'N':
		case ' ':
		case '+':
		case '=':
			if (c == 'f' || c == 'F')
				Put(usr, "Forward\n");
			else
				Put(usr, "Next\n");

			if (usr->history_p == NULL) {
				Perror(usr, "Your history buffer is gone");
				usr->runtime_flags &= ~RTF_BUSY;
				RET(usr);
				Return;
			}
			usr->history_p = usr->history_p->next;
			if (usr->history_p == NULL) {
				usr->runtime_flags &= ~RTF_BUSY;
				RET(usr);
				Return;
			}
			print_buffered_msg(usr, usr->history_p);
			break;

		case 'r':
		case 'v':
			Put(usr, "Reply\n");
			goto History_Reply_Code;

		case 'R':
		case 'V':
		case 'A':
		case 'a':
			Put(usr, "Reply to All\n");

History_Reply_Code:
			if (usr->history_p == NULL) {
				Put(usr, "<red>No message to reply to\n");
				usr->runtime_flags &= ~RTF_BUSY;
				RET(usr);
				Return;
			}
			listdestroy_StringList(usr->recipients);
			usr->recipients = NULL;

			if (c == 'R' || c == 'V' || c == 'A' || c == 'a') {
				if ((usr->recipients = copy_StringList(usr->history_p->to)) == NULL) {
					Perror(usr, "Out of memory");

					listdestroy_StringList(usr->recipients);
					usr->recipients = NULL;
				}
			}
			if ((sl = in_StringList(usr->recipients, usr->history_p->from)) == NULL) {
				if ((sl = new_StringList(usr->history_p->from)) == NULL) {
					Perror(usr, "Out of memory");
				} else
					concat_StringList(&usr->recipients, sl);
			}
			check_recipients(usr);
			if (usr->recipients == NULL)
				break;

			usr->edit_pos = 0;
			usr->runtime_flags |= RTF_BUSY;
			usr->edit_buf[0] = 0;

			if (usr->recipients->next == NULL && usr->recipients->prev == NULL)
				usr->runtime_flags &= ~RTF_MULTI;

			Print(usr, "\n<green>Replying to%s\n", print_many(usr));

			if (usr->history_p->flags & BUFMSG_EMOTE) {
				CALL(usr, STATE_EDIT_EMOTE);
			} else {
				CALL(usr, STATE_EDIT_X);
			}
			Return;

		case 'l':
		case 'L':
		case KEY_CTRL('L'):
			Put(usr, "List recipients\n");

			if (usr->history_p == NULL) {
				Perror(usr, "Your history buffer is gone");
				usr->runtime_flags &= ~RTF_BUSY;
				RET(usr);
				Return;
			}
			if (usr->history_p->to != NULL && usr->history_p->to->next != NULL)
				Put(usr, "<magenta>Recipients are <yellow>");
			else
				Put(usr, "<magenta>The recipient is <yellow>");

			for(sl = usr->history_p->to; sl != NULL; sl = sl->next) {
				if (sl->next == NULL)
					Print(usr, "%s\n", sl->str);
				else
					Print(usr, "%s, ", sl->str);
			}
			break;

		case 's':
		case 'S':
		case 'q':
		case 'Q':
		case KEY_ESC:
		case KEY_CTRL('C'):
		case KEY_CTRL('D'):
			if (c == 'q' || c == 'Q')
				Put(usr, "Quit\n");
			else
				Put(usr, "Stop\n");
			usr->runtime_flags &= ~RTF_BUSY;
			RET(usr);
			Return;
	}
	if (usr->history_p == NULL) {
		usr->runtime_flags &= ~RTF_BUSY;
		RET(usr);
		Return;
	}
	prompt[0] = 0;
	if (usr->history_p->prev != NULL)
		strcat(prompt, "<hotkey>Back, ");
	if (usr->history_p->next != NULL)
		strcat(prompt, "<hotkey>Next, ");
	if (strcmp(usr->history_p->from, usr->name))
		strcat(prompt, "<hotkey>Reply, ");
	if (usr->history_p->to != NULL && usr->history_p->to->next != NULL)
		strcat(prompt, "reply <hotkey>All, <hotkey>List recipients, ");

	Print(usr, "\n<green>%s<hotkey>Stop<white>: ", prompt);
	Return;
}

/*
	for messages that were held
	pretty much the same function as above, but now for held_msgp
	instead of history_p
*/
void state_held_history_prompt(User *usr, char c) {
StringList *sl;
char prompt[PRINT_BUF];

	if (usr == NULL)
		return;

	Enter(state_held_history_prompt);

	switch(c) {
		case INIT_STATE:
			if (usr->held_msgp == NULL)
				usr->held_msgp = usr->held_msgs;

			while(usr->held_msgp != NULL) {
				if (usr->held_msgp->flags & (BUFMSG_XMSG | BUFMSG_EMOTE | BUFMSG_FEELING | BUFMSG_QUESTION))
					break;
				usr->held_msgp = usr->held_msgp->prev;
			}
			if (usr->held_msgp == NULL) {
				usr->runtime_flags &= ~RTF_BUSY;
				RET(usr);
				Return;
			}
			print_buffered_msg(usr, usr->held_msgp);

			usr->runtime_flags |= RTF_BUSY;
			break;

		case 'b':
		case 'B':
		case 'p':
		case 'P':
		case KEY_BS:
		case '-':
		case '_':
			if (c == 'p' || c == 'P')
				Put(usr, "Previous\n");
			else
				Put(usr, "Back\n");

			if (usr->held_msgp == NULL) {
				Perror(usr, "Your held messages buffer is gone");
				usr->runtime_flags &= ~RTF_BUSY;
				RET(usr);
				Return;
			}
			if (usr->held_msgp->prev == NULL) {
				Put(usr, "<red>Can't go further back\n");
				break;
			}
			usr->held_msgp = usr->held_msgp->prev;
			print_buffered_msg(usr, usr->held_msgp);
			break;

		case 'f':
		case 'F':
		case 'n':
		case 'N':
		case ' ':
		case '+':
		case '=':
			if (c == 'f' || c == 'F')
				Put(usr, "Forward\n");
			else
				Put(usr, "Next\n");

			if (usr->held_msgp == NULL) {
				Perror(usr, "Your held messages buffer is gone");
				usr->runtime_flags &= ~RTF_BUSY;
				RET(usr);
				Return;
			}
			usr->held_msgp = usr->held_msgp->next;
			if (usr->held_msgp == NULL)
				goto Exit_Held_History;

			print_buffered_msg(usr, usr->held_msgp);
			break;

		case 'r':
		case 'v':
			Put(usr, "Reply\n");
			goto Held_History_Reply;

		case 'R':
		case 'V':
		case 'A':
		case 'a':
			Put(usr, "Reply to All\n");

Held_History_Reply:
			if (usr->held_msgp == NULL) {
				Put(usr, "<red>No message to reply to\n");
				usr->runtime_flags &= ~RTF_BUSY;
				RET(usr);
				Return;
			}
			listdestroy_StringList(usr->recipients);
			usr->recipients = NULL;

			if (c == 'R' || c == 'V' || c == 'A' || c == 'a') {
				if ((usr->recipients = copy_StringList(usr->held_msgp->to)) == NULL) {
					Perror(usr, "Out of memory");

					listdestroy_StringList(usr->recipients);
					usr->recipients = NULL;
				}
			}
			if ((sl = in_StringList(usr->recipients, usr->held_msgp->from)) == NULL) {
				if ((sl = new_StringList(usr->held_msgp->from)) == NULL) {
					Perror(usr, "Out of memory");
				} else
					concat_StringList(&usr->recipients, sl);
			}
			check_recipients(usr);
			if (usr->recipients == NULL)
				break;

			usr->edit_pos = 0;
			usr->runtime_flags |= RTF_BUSY;
			usr->edit_buf[0] = 0;

			if (usr->recipients->next == NULL && usr->recipients->prev == NULL)
				usr->runtime_flags &= ~RTF_MULTI;

			Print(usr, "\n<green>Replying to%s\n", print_many(usr));

			if (usr->held_msgp->flags & BUFMSG_EMOTE) {
				CALL(usr, STATE_EDIT_EMOTE);
			} else {
				CALL(usr, STATE_EDIT_X);
			}
			Return;

		case 'l':
		case 'L':
		case KEY_CTRL('L'):
			Put(usr, "List recipients\n");

			if (usr->held_msgp == NULL) {
				Perror(usr, "Your held messages buffer is gone");
				usr->runtime_flags &= ~RTF_BUSY;
				RET(usr);
				Return;
			}
			if (usr->held_msgp->to != NULL && usr->held_msgp->to->next != NULL)
				Put(usr, "<magenta>Recipients are <yellow>");
			else
				Put(usr, "<magenta>The recipient is <yellow>");

			for(sl = usr->held_msgp->to; sl != NULL; sl = sl->next) {
				if (sl->next == NULL)
					Print(usr, "%s\n", sl->str);
				else
					Print(usr, "%s, ", sl->str);
			}
			break;

		case 's':
		case 'S':
		case 'q':
		case 'Q':
		case KEY_ESC:
		case KEY_CTRL('C'):
		case KEY_CTRL('D'):
			if (c == 'q' || c == 'Q')
				Put(usr, "Quit\n");
			else
				Put(usr, "Stop\n");

Exit_Held_History:
			concat_BufferedMsg(&usr->history, usr->held_msgs);
			usr->held_msgs = usr->held_msgp = NULL;

			usr->runtime_flags &= ~(RTF_BUSY|RTF_HOLD);

			if (usr->runtime_flags & RTF_WAS_HH) {
				usr->runtime_flags &= ~RTF_WAS_HH;
				usr->flags |= USR_HELPING_HAND;
			}
			RET(usr);
			Return;
	}
	if (usr->held_msgp == NULL) {
		usr->runtime_flags &= ~RTF_BUSY;
		RET(usr);
		Return;
	}
	prompt[0] = 0;
	if (usr->held_msgp->prev != NULL)
		strcat(prompt, "<hotkey>Back, ");
	if (usr->held_msgp->next != NULL)
		strcat(prompt, "<hotkey>Next, ");
	if (strcmp(usr->held_msgp->from, usr->name))
		strcat(prompt, "<hotkey>Reply, ");
	if (usr->held_msgp->to != NULL && usr->held_msgp->to->next != NULL)
		strcat(prompt, "reply <hotkey>All, <hotkey>List recipients, ");

	Print(usr, "\n<green>%s<hotkey>Stop<white>: ", prompt);
	Return;
}

void read_more(User *usr) {
	Enter(read_more);

	usr->textp = usr->more_text = rewind_StringList(usr->more_text);
	usr->read_lines = 0;
	usr->total_lines = list_Count(usr->more_text);

	CALL(usr, STATE_MORE_PROMPT);

	Return;
}

void state_more_prompt(User *usr, char c) {
int l;
StringList *sl;

	if (usr == NULL)
		return;

	Enter(state_more_prompt);

	Put(usr, "<cr>                                      <cr><green>");
	switch(c) {
		case INIT_STATE:
			if (usr->more_text == NULL) {
				RET(usr);
				Return;
			}
			l = 0;
			for(sl = usr->textp; l < usr->term_height-1 && sl != NULL;) {
				Print(usr, "%s\n", sl->str);
				sl = sl->next;
				usr->read_lines++;
				l++;
			}
			usr->textp = sl;
			usr->runtime_flags |= RTF_BUSY;
			break;

		case 'b':
		case 'B':
			for(l = 0; l < (usr->term_height * 2); l++) {
				if (usr->textp->prev != NULL) {
					usr->textp = usr->textp->prev;
					if (usr->read_lines)
						usr->read_lines--;
				} else {
					if (l <= usr->term_height)
						l = -1;			/* user that's keeping 'b' pressed */
					break;
				}
			}
			if (l == -1) {				/* so bail out of the --More-- prompt */
				usr->textp = NULL;
				break;
			}

		case ' ':
		case 'n':
		case 'N':
			for(l = 0; l < usr->term_height && usr->textp != NULL; l++) {
				Print(usr, "%s\n", usr->textp->str);
				usr->read_lines++;
				usr->textp = usr->textp->next;
			}
			break;

		case KEY_RETURN:
		case '+':
		case '=':
			if (usr->textp->str != NULL)
				Print(usr, "%s\n", usr->textp->str);
			usr->textp = usr->textp->next;
			usr->read_lines++;
			break;

		case KEY_BS:
		case '-':
		case '_':
			for(l = 0; l < (usr->term_height+1); l++) {
				if (usr->textp->prev != NULL) {
					usr->textp = usr->textp->prev;
					if (usr->read_lines)
						usr->read_lines--;
				} else
					break;
			}
			for(l = 0; l < usr->term_height; l++) {
				if (usr->textp != NULL)
					Print(usr, "%s\n", usr->textp->str);
				else
					break;
				usr->read_lines++;
				usr->textp = usr->textp->next;
			}
			break;

		case 'g':						/* goto beginning */
			usr->textp = usr->more_text;
			usr->read_lines = 0;

			for(l = 0; l < usr->term_height; l++) {
				if (usr->textp != NULL)
					Print(usr, "%s\n", usr->textp->str);
				else
					break;
				usr->read_lines++;
				usr->textp = usr->textp->next;
			}
			break;

		case 'G':						/* goto end ; display last page */
			if (usr->textp == NULL)
				break;

/* goto the end */
			for(sl = usr->textp; sl != NULL && sl->next != NULL; sl = sl->next)
				usr->read_lines++;

/* go one screen back */
			l = 0;
			while(sl != NULL && sl->prev != NULL && l < usr->term_height) {
				sl = sl->prev;
				l++;
			}
			usr->textp = sl;
			usr->read_lines -= l;

/* display it */
			for(l = 0; l < usr->term_height; l++) {
				if (usr->textp != NULL)
					Print(usr, "%s\n", usr->textp->str);
				else
					break;
				usr->read_lines++;
				usr->textp = usr->textp->next;
			}
			break;

		case '/':						/* find */
			if (usr->textp == NULL)
				break;

			CALL(usr, STATE_MORE_FIND_PROMPT);
			Return;

		case '?':						/* find backwards */
			if (usr->textp == NULL)
				break;

			CALL(usr, STATE_MORE_FINDBACK_PROMPT);
			Return;

		case 'q':
		case 'Q':
		case 's':
		case 'S':
		case KEY_CTRL('C'):
		case KEY_CTRL('D'):
		case KEY_ESC:
			usr->textp = NULL;
			break;

		default:
			Put(usr, "<green>Press <white><<yellow>space<white>><green> for next page, <white><<yellow>b<white>><green> for previous page, <white><<yellow>enter<white>><green> for next line\n");
	}
	if (usr->textp != NULL)
		Print(usr, "<white>--<yellow>More<white>-- (<cyan>line %d<white>/<cyan>%d %d<white>%%)", usr->read_lines, usr->total_lines,
			100 * usr->read_lines / usr->total_lines);
	else {
/*
	Don't destroy in order to be able to reply to a message

		destroy_Message(usr->message);
		usr->message = NULL;
*/
		listdestroy_StringList(usr->more_text);
		usr->more_text = usr->textp = NULL;
		usr->read_lines = usr->total_lines = 0;
		RET(usr);
	}
	Return;
}

void state_more_find_prompt(User *usr, char c) {
StringList *sl;
int r, l;

	if (usr == NULL)
		return;

	Enter(state_more_find_prompt);

	if (c == INIT_STATE)
		Put(usr, "<white>Find: ");

	r = edit_line(usr, c);

	if (r == EDIT_BREAK) {
		l = 0;
		for(sl = usr->textp; sl != NULL && sl->prev != NULL && l < usr->term_height; sl = sl->prev)
			l++;
		usr->textp = sl;
		usr->read_lines -= l;
		RET(usr);
		Return;
	}
	if (r == EDIT_RETURN) {
		Put(usr, "<cr>                             <cr>");
		if (!usr->edit_buf[0]) {
			l = 0;
			for(sl = usr->textp; sl != NULL && sl->prev != NULL && l < usr->term_height; sl = sl->prev)
				l++;
			usr->textp = sl;
			usr->read_lines -= l;
			RET(usr);
			Return;
		}
/* always search from the top */
		l = 0;
		for(sl = usr->textp; sl != NULL && sl->prev != NULL && l < usr->term_height-1; sl = sl->prev)
			l++;

		l = -l;
		for(; sl != NULL; sl = sl->next) {
			l++;
			if (cstrstr(sl->str, usr->edit_buf) != NULL) {
				usr->textp = sl;
				l--;						/* error correction (?) */
				usr->read_lines += l;
				RET(usr);
				Return;
			}
		}
		MOV(usr, STATE_MORE_PROMPT);
		CALL(usr, STATE_MORE_NOTFOUND);
	}
	Return;
}

void state_more_findback_prompt(User *usr, char c) {
StringList *sl;
int r, l;

	if (usr == NULL)
		return;

	Enter(state_more_findback_prompt);

	if (c == INIT_STATE)
		Put(usr, "<white>Find (backwards): ");

	r = edit_line(usr, c);

	if (r == EDIT_BREAK) {
		l = 0;
		for(sl = usr->textp; sl != NULL && sl->prev != NULL && l < usr->term_height; sl = sl->prev)
			l++;
		usr->textp = sl;
		usr->read_lines -= l;
		RET(usr);
		Return;
	}
	if (r == EDIT_RETURN) {
		Put(usr, "<cr>                             <cr>");
		if (!usr->edit_buf[0]) {
			l = 0;
			for(sl = usr->textp; sl != NULL && sl->prev != NULL && l < usr->term_height; sl = sl->prev)
				l++;
			usr->textp = sl;
			usr->read_lines -= l;
			RET(usr);
			Return;
		}
/* always search from the top */
		l = 0;
		for(sl = usr->textp; sl != NULL && sl->prev != NULL && l < (usr->term_height+1); sl = sl->prev)
			l++;

		for(; sl != NULL; sl = sl->prev) {
			l++;
			if (cstrstr(sl->str, usr->edit_buf) != NULL) {
				usr->textp = sl;
				l--;						/* correction */
				usr->read_lines -= l;
				RET(usr);
				Return;
			}
		}
		MOV(usr, STATE_MORE_PROMPT);
		CALL(usr, STATE_MORE_NOTFOUND);
	}
	Return;
}

void state_more_notfound(User *usr, char c) {
	if (usr == NULL)
		return;

	Enter(state_more_notfound);

	if (c == INIT_STATE) {
		Print(usr, "<red>Not found");
		usr->runtime_flags |= RTF_BUSY;
		Return;
	}
	if (c == KEY_RETURN || c == ' ' || c == KEY_BS || c == KEY_CTRL('H')
		|| c == KEY_CTRL('C') || c == KEY_CTRL('D') || c == KEY_ESC) {
		StringList *sl;
		int l = 0;

		for(sl = usr->textp; sl != NULL && sl->prev != NULL && l < usr->term_height; sl = sl->prev)
			l++;
		usr->textp = sl;
		usr->read_lines -= l;
		RET(usr);
	} else
		Put(usr, "<beep>");
	Return;
}


void state_enter_forward_recipients(User *usr, char c) {
	if (usr == NULL)
		return;

	Enter(state_enter_forward_recipients);

	switch(edit_recipients(usr, c, multi_mail_access)) {
		case EDIT_BREAK:
			RET(usr);
			break;

		case EDIT_RETURN:
			if (usr->recipients == NULL) {
				RET(usr);
				break;
			}
			if (usr->new_message == NULL) {
				Perror(usr, "The message to forward has disappeared (?)");
				RET(usr);
				break;
			}
			usr->new_message->flags |= MSG_FORWARDED;
			usr->new_message->flags &= ~MSG_REPLY;

			listdestroy_StringList(usr->new_message->to);
			if ((usr->new_message->to = copy_StringList(usr->recipients)) == NULL) {
				Perror(usr, "Out of memory");
				RET(usr);
				break;
			}
			listdestroy_StringList(usr->recipients);
			usr->recipients = NULL;

			usr->new_message->mtime = rtc;

/* swap these pointers for save_message() */
			listdestroy_StringList(usr->more_text);
			usr->more_text = usr->new_message->msg;
			usr->new_message->msg = NULL;

			Put(usr, "<green>Message forwarded\n");

			save_message(usr, INIT_STATE);		/* send the forwarded Mail> */
/*			RET(usr);	save_message() already RET()s for us */
	}
	Return;
}

void state_forward_room(User *usr, char c) {
int r;

	if (usr == NULL)
		return;

	Enter(state_forward_room);

	if (c == INIT_STATE)
		Put(usr, "<green>Enter room name<yellow>: ");

	r = edit_roomname(usr, c);

	if (r == EDIT_BREAK) {
		Put(usr, "<red>Message not forwarded\n");
		RET(usr);
		Return;
	}
	if (r == EDIT_RETURN) {
		Room *r, *curr_room;

		if (!usr->edit_buf[0]) {
			RET(usr);
			Return;
		}
		if ((r = find_abbrevRoom(usr, usr->edit_buf)) == NULL) {
			Put(usr, "<red>No such room\n");
			RET(usr);
			Return;
		}
		if (r == usr->curr_room) {
			Put(usr, "<red>The message already is in this room\n");
			RET(usr);
			Return;
		}
		if (r->flags & ROOM_CHATROOM) {
			Put(usr, "<red>You cannot forward a message to a <white>chat<red> room\n");
			unload_Room(r);
			RET(usr);
			Return;
		}
		if (!(usr->runtime_flags & RTF_SYSOP)) {
			switch(room_access(r, usr->name)) {
				case ACCESS_INVITE_ONLY:
					Put(usr, "<red>That room is invite-only, and you have not been invited\n");
					unload_Room(r);
					RET(usr);
					Return;

				case ACCESS_KICKED:
					Put(usr, "<red>You have been kicked from that room\n");
					unload_Room(r);
					RET(usr);
					Return;

				case ACCESS_INVITED:
				case ACCESS_OK:
					break;
			}
		}
		if ((r->flags & ROOM_READONLY)
			&& !((usr->runtime_flags & RTF_SYSOP)
			|| (in_StringList(r->room_aides, usr->name) != NULL))) {
			Put(usr, "<red>You are not allowed to post in that room\n");
			unload_Room(r);
			RET(usr);
			Return;
		}
		curr_room = usr->curr_room;
		usr->curr_room = r;

		if (!usr->new_message->subject[0]) {
			char buf[MAX_LINE*3];

			sprintf(buf, "<yellow><forwarded from %s>", curr_room->name);
			if (strlen(buf) >= MAX_LINE)
				strcpy(buf + MAX_LINE - 5, "...>");

			strcpy(usr->new_message->subject, buf);
		}
		usr->new_message->flags |= MSG_FORWARDED;
		usr->new_message->mtime = rtc;
		listdestroy_StringList(usr->new_message->to);
		if ((usr->new_message->to = new_StringList(usr->name)) == NULL) {
			Perror(usr, "Out of memory");
			usr->curr_room = curr_room;
			unload_Room(r);
			RET(usr);
			Return;
		}
/* swap these pointers for save_message() */
		listdestroy_StringList(usr->more_text);
		usr->more_text = usr->new_message->msg;
		usr->new_message->msg = NULL;

		PUSH(usr, STATE_DUMMY);			/* push dummy ret (prevent reprinting prompt in wrong room) */
		PUSH(usr, STATE_DUMMY);			/* one is not enough (!) */
		save_message(usr, INIT_STATE);	/* save forwarded message */
		usr->curr_room = curr_room;		/* restore current room */

		Put(usr, "<green>Message forwarded\n");

		unload_Room(r);
		RET(usr);
	}
	Return;
}

/*
	generally used after state_more_prompt()
*/
void state_press_any_key(User *usr, char c) {
	if (usr == NULL)
		return;

	Enter(state_press_any_key);

	if (c == INIT_STATE) {
		usr->runtime_flags |= RTF_BUSY;
		Put(usr, "<red>-- Press any key to continue --");
	} else {
		Put(usr, "<cr>                               <cr>");
		RET(usr);
	}
	Return;
}


void print_new_msg_header(User *usr) {
char from[MAX_LINE];

	if (usr == NULL)
		return;

	Enter(print_new_msg_header);

	Put(usr, "\n");
	if (usr->new_message == NULL) {
		Perror(usr, "I have a problem with this");
		Return;
	}
/* print message header */

	if (usr->new_message->anon[0])
		sprintf(from, "<cyan>- %s <cyan>-", usr->new_message->anon);
	else
		if (usr->new_message->flags & MSG_FROM_SYSOP)
			sprintf(from, "<yellow>%s<white>:<yellow> %s", PARAM_NAME_SYSOP, usr->new_message->from);
		else
			if (usr->new_message->flags & MSG_FROM_ROOMAIDE)
				sprintf(from, "<yellow>%s<white>:<yellow> %s", PARAM_NAME_ROOMAIDE, usr->new_message->from);
			else
				sprintf(from, "<yellow>%s", usr->new_message->from);

	if (usr->new_message->to != NULL) {
		StringList *sl;

		if (!strcmp(usr->new_message->from, usr->name)) {
			Print(usr, "<cyan>%s<green>, to ", print_date(usr, usr->new_message->mtime));

			for(sl = usr->new_message->to; sl != NULL && sl->next != NULL; sl = sl->next)
				Print(usr, "<yellow>%s<green>, ", sl->str);
			Print(usr, "<yellow>%s<green>\n", sl->str);
		} else {
			if (usr->new_message->to != NULL && usr->new_message->to->next == NULL && !strcmp(usr->new_message->to->str, usr->name))
				Print(usr, "<cyan>%s<green>, from %s<green>\n", print_date(usr, usr->new_message->mtime), from);
			else {
				Print(usr, "<cyan>%s<green>, from %s<green> to ", print_date(usr, usr->new_message->mtime), from);

				for(sl = usr->new_message->to; sl != NULL && sl->next != NULL; sl = sl->next)
					Print(usr, "<yellow>%s<green>, ", sl->str);
				Print(usr, "<yellow>%s<green>\n", sl->str);
			}
		}
	} else
		Print(usr, "<cyan>%s<green>, from %s<green>\n", print_date(usr, usr->new_message->mtime), from);
	Return;
}

void more_msg_header(User *usr) {
char from[MAX_LINE], buf[MAX_LINE*3];

	if (usr == NULL)
		return;

	Enter(more_msg_header);

	listdestroy_StringList(usr->more_text);
	if ((usr->more_text = new_StringList("")) == NULL) {
		Perror(usr, "Out of memory");
		Return;
	}
	if (usr->message == NULL) {
		Perror(usr, "I have a problem with this");
		Return;
	}
/* print message header */

	if (usr->message->anon[0])
		sprintf(from, "<cyan>- %s -", usr->message->anon);
	else
		if (usr->message->flags & MSG_FROM_SYSOP)
			sprintf(from, "<yellow>%s<white>:<yellow> %s", PARAM_NAME_SYSOP, usr->message->from);
		else
			if (usr->message->flags & MSG_FROM_ROOMAIDE)
				sprintf(from, "<yellow>%s<white>:<yellow> %s", PARAM_NAME_ROOMAIDE, usr->message->from);
			else
				sprintf(from, "<yellow>%s", usr->message->from);

	if (usr->message->to != NULL) {
		StringList *sl;
		int l, dl, max_dl;			/* l = strlen, dl = display length */

		max_dl = usr->term_width-1;
		if (max_dl >= (MAX_LINE*3-1))	/* MAX_LINE*3 is used buffer size */
			max_dl = MAX_LINE*3-1;

		if (!strcmp(usr->message->from, usr->name)) {
			l = sprintf(buf, "<cyan>%s<green>, to ", print_date(usr, usr->message->mtime));
			dl = color_strlen(buf);

			for(sl = usr->message->to; sl != NULL && sl->next != NULL; sl = sl->next) {
				if ((dl + strlen(sl->str)+2) < max_dl)
					l += sprintf(buf+l, "<yellow>%s<green>, ", sl->str);
				else {
					usr->more_text = add_StringList(&usr->more_text, new_StringList(buf));
					l = sprintf(buf, "<yellow>%s<green>, ", sl->str);
				}
				dl = color_strlen(buf);
			}
			add_String(&usr->more_text, "%s<yellow>%s<green>", buf, sl->str);
		} else {
			if (usr->message->to != NULL && usr->message->to->next == NULL && !strcmp(usr->message->to->str, usr->name))
				usr->more_text = add_String(&usr->more_text, "<cyan>%s<green>, from %s<green>", print_date(usr, usr->message->mtime), from);
			else {
				l = sprintf(buf, "<cyan>%s<green>, from %s<green> to ", print_date(usr, usr->message->mtime), from);
				dl = color_strlen(buf);

				for(sl = usr->message->to; sl != NULL && sl->next != NULL; sl = sl->next) {
					if ((dl + strlen(sl->str)+2) < MAX_LINE)
						l += sprintf(buf+strlen(buf), "<yellow>%s<green>, ", sl->str);
					else {
						usr->more_text = add_StringList(&usr->more_text, new_StringList(buf));
						l = sprintf(buf, "<yellow>%s<green>, ", sl->str);
					}
					dl = color_strlen(buf);
				}
				usr->more_text = add_String(&usr->more_text, "%s<yellow>%s<green>", buf, sl->str);
			}
		}
	} else
		usr->more_text = add_String(&usr->more_text, "<cyan>%s<green>, from %s<green>", print_date(usr, usr->message->mtime), from);
	Return;
}


/*
	Note: this routine does not work for the Mail> room
	      (use expire_mail())
*/
void expire_msg(Room *r) {
User *u;
MsgIndex *m;
char buf[MAX_PATHLEN];

	if (r == NULL || r->msgs == NULL || list_Count(r->msgs) <= PARAM_MAX_MESSAGES)
		return;

	Enter(expire_msg);

	for(u = AllUsers; u != NULL; u = u->next)
		if (u->curr_msg == r->msgs)
			u->curr_msg = NULL;			/* hmm... I wonder if strange things could happen here */

	sprintf(buf, "%s/%u/%lu", PARAM_ROOMDIR, r->number, r->msgs->number);
	path_strip(buf);
	unlink_file(buf);

	m = r->msgs;
	remove_MsgIndex(&r->msgs, m);
	destroy_MsgIndex(m);

	Return;
}

void expire_mail(User *usr) {
MsgIndex *m;
char buf[MAX_PATHLEN];
int i;

	if (usr == NULL || usr->mail == NULL || usr->mail->msgs == NULL
		|| (i = list_Count(usr->mail->msgs)) <= PARAM_MAX_MAIL_MSGS)
		return;

	Enter(expire_mail);

	while(i > PARAM_MAX_MAIL_MSGS) {
		i--;
		m = usr->mail->msgs;
		if (m == NULL)
			break;

		sprintf(buf, "%s/%c/%s/%lu", PARAM_USERDIR, usr->name[0], usr->name, m->number);
		path_strip(buf);
		unlink_file(buf);

		remove_MsgIndex(&usr->mail->msgs, m);
		destroy_MsgIndex(m);
	}
	Return;
}


/*
	find the next unread room
	this is an unseemingly costly operation
*/
Room *next_unread_room(User *usr) {
Room *r, *r2;
MsgIndex *m;
Joined *j;

	if (usr == NULL)
		return Lobby_room;

	if (usr->curr_room == NULL) {
		Perror(usr, "Suddenly your environment goes up in flames. Please re-login (?)");
		return Lobby_room;
	}
	if (usr->mail == NULL) {
		Perror(usr, "Your mailbox seems to have vaporized. Please re-login (?)");
		return Lobby_room;
	}
/*
	anything new in the Lobby?
*/
	if (unread_room(usr, Lobby_room) != NULL)
		return Lobby_room;

/* then check the mail room */

	if ((j = in_Joined(usr->rooms, 1)) == NULL) {
		if ((j = new_Joined()) == NULL) {			/* this should never happen, but hey... */
			Perror(usr, "Out of memory");
			return Lobby_room;
		}
		j->number = 1;
		if (usr->mail != NULL)
			j->generation = usr->mail->generation;
		add_Joined(&usr->rooms, j);
	}
	m = unwind_MsgIndex(usr->mail->msgs);
	if (m != NULL && m->number > j->last_read)
		return usr->mail;

/*
	scan for next unread rooms
	we search the current room, and we proceed our search from there
*/
	for(r = AllRooms; r != NULL; r = r->next) {
		if (r->number == usr->curr_room->number)
			break;
	}
	if (r == NULL) {								/* current room does not exist anymore */
		Perror(usr, "Your environment has vaporized!");
		return Lobby_room;
	}
/* we've seen the current room, skip to the next and search for an unread room */

	for(r = r->next; r != NULL; r = r->next) {
		if ((r2 = unread_room(usr, r)) != NULL)
			return r2;
	}
/* couldn't find a room, now search from the beginning up to curr_room */

	for(r = AllRooms; r != NULL && r->number != usr->curr_room->number; r = r->next) {
		if ((r2 = unread_room(usr, r)) != NULL)
			return r2;
	}
/* couldn't find an unread room; goto the Lobby> */
	return Lobby_room;
}

/*
	Checks if room r may be read and if it is unread
	This is a helper routine for next_unread_room()
*/
Room *unread_room(User *usr, Room *r) {
Joined *j;
MsgIndex *m;

	if (usr == NULL || r == NULL || (r->flags & ROOM_CHATROOM))
		return NULL;

	Enter(unread_room);

/*
	NOTE: we skip room #1 and #2 (because they are 'dynamic' in the User
	      and Mail> is checked seperately
	      (remove this check and strange things happen... :P)
*/
	if (r->number == 1 || r->number == 2) {
		Return NULL;
	}
	j = in_Joined(usr->rooms, r->number);

/* if not welcome anymore, unjoin and proceed */

	if (in_StringList(r->kicked, usr->name) != NULL
		|| ((r->flags & ROOM_INVITE_ONLY) && in_StringList(r->invited, usr->name) == NULL)
		|| ((r->flags & ROOM_HIDDEN) && j == NULL)
		|| ((r->flags & ROOM_HIDDEN) && j != NULL && r->generation != j->generation)
		) {
		if (j != NULL) {
			remove_Joined(&usr->rooms, j);
			destroy_Joined(j);
		}
		Return NULL;
	}
	if (j != NULL) {
		if (j->zapped) {				/* Zapped this public room, forget it */
			if (j->generation == r->generation) {
				Return NULL;
			}
			j->zapped = 0;				/* auto-unzap changed room */
		}
	} else {
		if (!(r->flags & ROOM_HIDDEN)) {
			if ((j = new_Joined()) == NULL) {			/* auto-join public room */
				Perror(usr, "Out of memory, room not joined");
				Return NULL;
			}
			j->number = r->number;
			add_Joined(&usr->rooms, j);
		}
	}
	if (j->generation != r->generation) {
		j->generation = r->generation;
		j->last_read = 0UL;				/* re-read changed room */
	}
	m = unwind_MsgIndex(r->msgs);
	if (m != NULL && m->number > j->last_read) {
		Return r;
	}
	Return NULL;
}

/*
	Send a message as Mail>
*/
void mail_msg(User *usr, BufferedMsg *msg) {
StringList *sl;
Room *room;
char *p, c;
int l;

	if (usr == NULL)
		return;

	Enter(mail_msg);

	if (msg == NULL) {
		Perror(usr, "The message is gone all of a sudden!");
		Return;
	}
	destroy_Message(usr->new_message);
	if ((usr->new_message = new_Message()) == NULL) {
		Perror(usr, "Out of memory");
		Return;
	}
	strcpy(usr->new_message->from, usr->name);

	sl = usr->recipients;
	if (sl != NULL) {
		unsigned long i;

		for(i = 0UL; i < usr->loop_counter; i++) {
			sl = sl->next;
			if (sl == NULL)
				break;
		}
	}
	if (sl == NULL) {
		Perror(usr, "The recipient has vanished!");
		Return;
	}
	if ((usr->new_message->to = new_StringList(sl->str)) == NULL) {
		Perror(usr, "Out of memory");
		Return;
	}
	strcpy(usr->new_message->subject, "<lost message>");

	listdestroy_StringList(usr->more_text);
	if ((usr->more_text = new_StringList("<cyan>Delivery of this message was impossible. You do get it this way.")) == NULL) {
		Perror(usr, "Out of memory");
		Return;
	}
	usr->more_text = add_StringList(&usr->more_text, new_StringList(" "));

/*
	This is the most ugliest hack ever; temporarely reset name to get a correct
	msg header out of buffered_msf_header();
	I really must rewrite this some day
*/
	c = usr->name[0];
	usr->name[0] = 0;
	p = buffered_msg_header(usr, usr->send_msg);
	usr->name[0] = c;

/* kludge for emotes, which have the message on the same line as the header :P */
	l = strlen(p)-1;
	if (l >= 0 && p[l] == '\n') {
		p[l] = 0;
		usr->more_text = add_StringList(&usr->more_text, new_StringList(p));
		usr->more_text = concat_StringList(&usr->more_text, copy_StringList(usr->send_msg->msg));
	} else {
		strcat(p, usr->send_msg->msg->str);
		usr->more_text = add_StringList(&usr->more_text, new_StringList(p));
		usr->more_text = concat_StringList(&usr->more_text, copy_StringList(usr->send_msg->msg->next));
	}
	room = usr->curr_room;
	usr->curr_room = usr->mail;

/* save the Mail> message */
	PUSH(usr, STATE_DUMMY);
	PUSH(usr, STATE_DUMMY);
	save_message(usr, INIT_STATE);

	destroy_Message(usr->new_message);
	usr->new_message = NULL;

	usr->curr_room = room;
	Return;
}

void room_beep(User *usr, Room *r) {
PList *p;
User *u;

	if (usr == NULL || r == NULL)
		return;

	Enter(room_beep);

	for(p = r->inside; p != NULL; p = p->next) {
		u = (User *)p->p;
		if (u == NULL || u == usr)
			continue;

		if (u->runtime_flags & RTF_BUSY)
			continue;

		Put(u, "<beep>");
	}
	Return;
}

/* EOB */
