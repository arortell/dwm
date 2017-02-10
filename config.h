/* See LICENSE file for copyright and license details. */

/* needed for audio keys */
#include <X11/XF86keysym.h>


/* appearance */
static const char *fonts[] = {
	"monospace:size=10"
};

//clean these up along with passcmd
//static const char normbordercolor[] = "#4000FF";
//static const char normbgcolor[]     = "#222222";
//static const char normfgcolor[]     = "#bbbbbb";
//static const char selbordercolor[]  = "#00BFFF";
//static const char selbgcolor[]      = "#005577";
//static const char selfgcolor[]      = "#eeeeee";

static const char dmenufont[]       = "monospace:size=10";
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const int window_gap         = 6;        /* gap between windows */

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */

static const Layout layouts[] = {
	/* symbol     arrange function */
    { "[]=", tile },    /* first entry is default */
    { "><>", NULL },    /* no layout function means floating behavior */
    { "[M]", monocle },
    {"TTT",  bstack },
    {"===",  bstackhoriz },
};

/* key definitions */
#define MODKEY Mod4Mask
#define ALTKEY Mod1Mask
#define NUMCOLORS  4

#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

#define STACKKEYS(MOD,ACTION) \
    { MOD, XK_j,     ACTION##stack, {.i = INC(+1) } }, \
    { MOD, XK_k,     ACTION##stack, {.i = INC(-1) } }, \
    { MOD, XK_grave, ACTION##stack, {.i = PREVSEL } }, \
    { MOD, XK_Left,  ACTION##stack, {.i = 0 } }, \
    { MOD, XK_Up,    ACTION##stack, {.i = 1 } }, \
    { MOD, XK_Down,  ACTION##stack, {.i = 2 } }, \
    { MOD, XK_Right, ACTION##stack, {.i = -1 } },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

static const char colors[NUMCOLORS][MAXCOLORS][8] = {
  // border   foreground background
  { "#4000FF", "#dddddd", "#222222" },  // normal
  { "#00BFFF", "#ffffff", "#000088" },  // selected
  { "#ff0000", "#000000", "#ffff00" },  // urgent/warning  (black on yellow)
  { "#ff0000", "#ffffff", "#ff0000" },  // error (white on red)
  // add more here
};

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[]     = { "dmenu_extended_run", NULL };
static const char *passcmd[]      = { "passmenu", NULL };
static const char *termcmd[]      = { "urxvtc", NULL };
static const char *downvolcmd[]   = { "amixer", "-q", "set", "Master", "2-", NULL };
static const char *upvolcmd[]     = { "amixer", "-q", "set", "Master", "2+", NULL };
static const char *mutevolcmd[]   = { "amixer", "-q", "set", "Master", "toggle", NULL };
static const char *lockcmd[]      = { "slock", NULL };
static const char *browsercmd[]   = { "qutebrowser", NULL };
static const char *torbrowcmd[]   = { "tor-browser", NULL };
static const char *qtcreatorcmd[] = { "qtcreator",  NULL };


static Key keys[] = {
    /* modifier                     key        function        argument */
    { MODKEY,                       XK_d,      spawn,          {.v = dmenucmd } },
    { MODKEY,			            XK_p,      spawn,          {.v = passcmd } },
    { MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
    { MODKEY,                       XK_q,      spawn,          {.v = browsercmd } },
    { MODKEY|ShiftMask,             XK_t,      spawn,          {.v = torbrowcmd } },
    { ALTKEY|ControlMask,           XK_l,      spawn,          {.v = lockcmd } },
    { MODKEY,                       XK_i,      spawn,          {.v = qtcreatorcmd } },
    { MODKEY|ShiftMask,             XK_b,      togglebar,      {0} },
    STACKKEYS(MODKEY,                          focus)
    STACKKEYS(MODKEY|ShiftMask,                push)
    { MODKEY,                       XK_plus,   setmfact,       {.f = -0.05} },
    { MODKEY,                       XK_minus,  setmfact,       {.f = +0.05} },
    { MODKEY,                       XK_Return, zoom,           {0} },
    { MODKEY,                       XK_Tab,    view,           {0} },
    { MODKEY,                       XK_Delete, killclient,     {0} },
    { MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
    { MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
    { MODKEY,                       XK_o,      setlayout,      {.v = &layouts[2]} },
    { MODKEY,                       XK_b,      setlayout,      {.v = &layouts[3]} },
    { MODKEY,                       XK_h,      setlayout,      {.v = &layouts[4]} },
    { MODKEY,                       XK_space,  setlayout,      {0} },
    { MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
    { MODKEY,                       XK_0,      view,           {.ui = ~0 } },
    { MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
    { MODKEY,                       XK_slash,  focusmon,       {.i = 1 } },
    { MODKEY,                       XK_m,      focusmon,       {.i = 3 } },
    { MODKEY,                       XK_comma,  focusmon,       {.i = 0 } },
    { MODKEY,                       XK_period, focusmon,       {.i = 2 } },
    { MODKEY|ShiftMask,             XK_slash,  tagmon,         {.i = 1 } },
    { MODKEY|ShiftMask,             XK_m,      tagmon,         {.i = 3 } },
    { MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = 0 } },
    { MODKEY|ShiftMask,             XK_period, tagmon,         {.i = 2 } },
    { 0,                            XF86XK_AudioLowerVolume, spawn, {.v = downvolcmd} },
    { 0,                            XF86XK_AudioRaiseVolume, spawn, {.v = upvolcmd} },
    { 0,                            XF86XK_AudioMute, spawn, {.v = mutevolcmd } },
    TAGKEYS(                        XK_1,                      0)
    TAGKEYS(                        XK_2,                      1)
    TAGKEYS(                        XK_3,                      2)
    TAGKEYS(                        XK_4,                      3)
    TAGKEYS(                        XK_5,                      4)
    TAGKEYS(                        XK_6,                      5)
    TAGKEYS(                        XK_7,                      6)
    TAGKEYS(                        XK_8,                      7)
    TAGKEYS(                        XK_9,                      8)
    { MODKEY|ShiftMask,             XK_q,      quit,           {0} },
};

/* button definitions */
/* click can be ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};

