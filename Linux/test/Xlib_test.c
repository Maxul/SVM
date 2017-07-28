
#include "stdio.h"
#include "stdlib.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>

/*
   A most basic (but damn useful) X windows demonstration.
   Compile as
      cc -o xdemo xdemo.c -lX11
*/

int main(int argc,char **argv)
{
    int i;
    int blackColor,whiteColor,thescreen;
    char s[64];
    XTextProperty textproperty;
    Display *thedisplay;
    GC thecontext;
    Font thefont;
    XEvent anevent;
    XColor xcolour;
    XPoint v[3];
    Colormap thecolormap;
    Window thewindow;

    thedisplay  = XOpenDisplay(NULL);
    blackColor  = BlackPixel(thedisplay,DefaultScreen(thedisplay));
    whiteColor  = WhitePixel(thedisplay,DefaultScreen(thedisplay));
    thescreen   = DefaultScreen(thedisplay);
    thecolormap = DefaultColormap(thedisplay,thescreen);

    /* Create the window */
    thewindow = XCreateSimpleWindow(thedisplay,
                                    DefaultRootWindow(thedisplay),0,0,
                                    400,400,0,blackColor,blackColor);
    XSelectInput(thedisplay,thewindow,StructureNotifyMask);
    XMapWindow(thedisplay,thewindow);

    /* Label the window */
    char *title = "终端";
    //printf("%lu\n", strlen(title));
    XStringListToTextProperty(&(title),1,&textproperty);
    printf("A %lu v %s f %d n %lu\n", textproperty.encoding,
           textproperty.value, textproperty.format, textproperty.nitems);
    XSetWMName(thedisplay,thewindow,&textproperty);

    /* Get the context */
    thecontext = XCreateGC(thedisplay,thewindow,0,NULL);
    XSetBackground(thedisplay,thecontext,blackColor);
    XSetForeground(thedisplay,thecontext,whiteColor);

    /* Wait for the MapNotify event */
    for (;;) {
        XNextEvent(thedisplay, &anevent);
        if (anevent.type == MapNotify)
            break;
    }

    /* Erase the display (In the background colour) */
    XClearWindow(thedisplay,thewindow);

    /* Draw a line (In the foreground colour) */
    XDrawLine(thedisplay,thewindow,thecontext,10,10,390,390);
    XFlush(thedisplay);

    /* Draw a green filled rectangle */
    xcolour.red = 32000;
    xcolour.green = 65000;
    xcolour.blue = 32000;
    xcolour.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(thedisplay,thecolormap,&xcolour);
    XSetForeground(thedisplay,thecontext,xcolour.pixel);
    XFillRectangle(thedisplay,thewindow,thecontext,100,100,200,200);
    XFlush(thedisplay);

    /* Draw a red filled polygon with a black border */
    v[0].x = 150;
    v[0].y = 250;
    v[1].x = 250;
    v[1].y = 250;
    v[2].x = 200;
    v[2].y = 150;
    xcolour.red = 65535;
    xcolour.green = 10000;
    xcolour.blue = 10000;
    xcolour.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(thedisplay,thecolormap,&xcolour);
    XSetForeground(thedisplay,thecontext,xcolour.pixel);
    XFillPolygon(thedisplay,thewindow,thecontext,v,3,Complex,CoordModeOrigin);
    XSetForeground(thedisplay,thecontext,blackColor);
    for (i=0; i<3; i++) {
        XDrawLine(thedisplay,thewindow,thecontext,
                  v[i].x,v[i].y,v[(i+1)%3].x,v[(i+1)%3].y);
    }
    XFlush(thedisplay);

    /* Draw a blue 90 degree arc */
    xcolour.red = 10000;
    xcolour.green = 10000;
    xcolour.blue = 65535;
    xcolour.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(thedisplay,thecolormap,&xcolour);
    XSetForeground(thedisplay,thecontext,xcolour.pixel);
    XFillArc(thedisplay,thewindow,thecontext,200,120,80,80,0*64,90*64);
    XFlush(thedisplay);

    /* Draw some text in black */
    strcpy(s,"xdemo.c");
    thefont = XLoadFont(thedisplay,"9x15");
    XSetForeground(thedisplay,thecontext,blackColor);
    XDrawString(thedisplay,thewindow,thecontext,120,120,s,strlen(s));
    XFlush(thedisplay);

    /* Done */
    usleep(1000000);
    XFreeGC(thedisplay,thecontext);
    XUnmapWindow(thedisplay,thewindow);
    XDestroyWindow(thedisplay,thewindow);
    XCloseDisplay(thedisplay);
}


