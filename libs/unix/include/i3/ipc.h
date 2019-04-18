/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * This public header defines the different constants and message types to use
 * for the IPC interface to i3 (see docs/ipc for more information).
 *
 */

#ifndef _I3_IPC_H
#define _I3_IPC_H

/*
 * Messages from clients to i3
 *
 */

/** Never change this, only on major IPC breakage (don’t do that) */
#define I3_IPC_MAGIC 			"i3-ipc"

/** The payload of the message will be interpreted as a command */
#define I3_IPC_MESSAGE_TYPE_COMMAND	        0

/** Requests the current workspaces from i3 */
#define I3_IPC_MESSAGE_TYPE_GET_WORKSPACES      1

/** Subscribe to the specified events */
#define I3_IPC_MESSAGE_TYPE_SUBSCRIBE           2

/** Requests the current outputs from i3 */
#define I3_IPC_MESSAGE_TYPE_GET_OUTPUTS         3

/** Requests the tree layout from i3 */
#define I3_IPC_MESSAGE_TYPE_GET_TREE            4


/*
 * Messages from i3 to clients
 *
 */

/** Command reply type */
#define I3_IPC_REPLY_TYPE_COMMAND               0

/** Workspaces reply type */
#define I3_IPC_REPLY_TYPE_WORKSPACES            1

/** Subscription reply type */
#define I3_IPC_REPLY_TYPE_SUBSCRIBE             2

/** Outputs reply type */
#define I3_IPC_REPLY_TYPE_OUTPUTS               3

/** Tree reply type */
#define I3_IPC_REPLY_TYPE_TREE                  4


/*
 * Events from i3 to clients. Events have the first bit set high.
 *
 */
#define I3_IPC_EVENT_MASK                       (1 << 31)

/* The workspace event will be triggered upon changes in the workspace list */
#define I3_IPC_EVENT_WORKSPACE                  (I3_IPC_EVENT_MASK | 0)

/* The output event will be triggered upon changes in the output list */
#define I3_IPC_EVENT_OUTPUT                     (I3_IPC_EVENT_MASK | 1)

#endif
