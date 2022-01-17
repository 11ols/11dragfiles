/** drag file/files from Max, mousebutton needs to be down already when drag() is called
-- 11olsen.de
*/

#include "ext.h"							
#include "ext_obex.h"						

#ifdef WIN_VERSION
#include "windows.h"
#include "Win/DragAndDrop.h"
#include "shlobj.h"
#else
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

@interface DragSource : NSObject
@end

@implementation DragSource
- (void)draggedImage:(NSImage*)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)aDragOperation
{
    // todo: mouse cursor reset
    //[[NSCursor arrowCursor] set];
}
@end
#endif


////////////////////////// object struct
typedef struct _dragfiles
{
	t_object					ob;			// the object itself (must be first)
} t_dragfiles;

///////////////////////// function prototypes
//// standard set
void *dragfiles_new(t_symbol *s, long argc, t_atom *argv);
void dragfiles_free(t_dragfiles *x);
void dragfiles_assist(t_dragfiles *x, void *b, long m, long a, char *s);

void dragfiles_drag(t_dragfiles *x, t_symbol *s, long argc, t_atom *argv);

//////////////////////// global class pointer variable
void *dragfiles_class;


int C74_EXPORT main(void)
{
	t_class *c;
    common_symbols_init();
    
	c = class_new("11dragfiles", (method)dragfiles_new, (method)dragfiles_free, (long)sizeof(t_dragfiles), 0L, A_GIMME, 0);


	class_addmethod(c, (method)dragfiles_assist, "assist", A_CANT, 0);
	class_addmethod(c, (method)dragfiles_drag, "drag", A_GIMME, 0);

	//class_addmethod(c, (method)dragfiles_bang,		"bang",		0);

	class_register(CLASS_BOX, c); /* CLASS_NOBOX */
	dragfiles_class = c;
    
    #ifdef WIN_VERSION
	MyDragDropInit(NULL);
    #endif
    
	object_post(NULL, "11dragfiles 2022/01/17 11OLSEN.DE");
	return 0;
}


#ifdef WIN_VERSION
void dragfiles_drag(t_dragfiles *x, t_symbol *s, long argc, t_atom *argv)
{
	CLIPFORMAT cf[1] = { CF_HDROP };
	HGLOBAL hData[1];
	//WNDPROC pProc;
	PMYDROPSOURCE pSrc;
	//size_t sz;
	HGLOBAL hGlobal;
	LPSTR pFileList;


	int i;
	t_uint16 *files; // the final wcstring with all filenames
	long filesStringSize = 0; // the size of *files, trailing \0 inclusive 


	//////// LETS GO  ////

	//early-exit-checks
	if (argc == 0) return;

	for (i = 0; i < argc; i++)
	{
		if (atom_gettype(argv + i) != A_SYM)
		{
			object_error((t_object *)x, "wrong input, use list of files (full path)");
			return;
		}
	}


	// dynamic wide char string builder
	for (i = 0; i < argc; i++) // collect converted utf8 strings into one big unicode string 
	{
		char from[MAX_PATH_CHARS]; //to hold the utf8 string
		t_uint16 *temp_unicode_string; // to hold wide char string
		long tmpUniStrLen;

		// need that backslash in Windows paths
		path_nameconform(atom_getsym(argv + i)->s_name, from, PATH_STYLE_NATIVE, PATH_TYPE_BOOT);

		// utf8 to unicode
		temp_unicode_string = charset_utf8tounicode(from, &tmpUniStrLen);

		//object_post((t_object *)x, "filepath %i has length of %i wchars (without \\0)", i + 1, tmpUniStrLen);

		if (i == 0) // first file in list
		{
			files = (t_uint16 *)calloc(tmpUniStrLen + 1 + 1, sizeof(t_uint16));   // +1 for separator // +1 for \0
			wcsncpy(files, temp_unicode_string, tmpUniStrLen + 1); // copy wide string and it's termination
			wcscat(files, L"|"); // append separator
			filesStringSize += tmpUniStrLen + 1 + 1; // new length
		}
		else // more files
		{
			files = (t_uint16 *)realloc(files, (filesStringSize + tmpUniStrLen + 1) * sizeof(t_uint16)); // enlarge our final string
			wcscat(files, temp_unicode_string); // append string
			wcscat(files, L"|"); // append separator
			filesStringSize += tmpUniStrLen + 1; // new length
		}

		sysmem_freeptr(temp_unicode_string);
	}

	// print the complete String to Max window
	//object_post((t_object *)x, "%s", charset_unicodetoutf8(files, wcslen(files), NULL) );

	// replace separator
	for (i = 0; i < filesStringSize - 1; i++)
	{
		if (files[i] == L'|')
			files[i] = L'\0'; // add NULL's as separator
	}


	// now, we allocate some memory to hold our list of files _and_our DROPFILES structure.
	hGlobal = GlobalAlloc(GMEM_SHARE, filesStringSize*sizeof(t_uint16) + sizeof(DROPFILES));

	// if we got the memory, we'll lock it down and construct the proper DROPFILES structure.
	if (hGlobal != NULL)
	{
		DROPFILES* pDropFiles = (DROPFILES*)GlobalLock(hGlobal);
		if (pDropFiles != NULL)
		{
			// init all structure members
			pDropFiles->pFiles = sizeof(DROPFILES);
			pDropFiles->pt.x = 0;
			pDropFiles->pt.y = 0;
			pDropFiles->fNC = FALSE;
			pDropFiles->fWide = TRUE;

			// find a pointer to the first CHAR after the structure
			// and copy our list of files there. Again, note that
			// strcpy() won't work because of the nulls in the string.

			pFileList = (LPSTR)pDropFiles + pDropFiles->pFiles;
			memcpy(pFileList, files, filesStringSize*sizeof(t_uint16));

			GlobalUnlock(hGlobal);
			hData[0] = hGlobal;

			pSrc = CreateMyDropSource(FALSE, cf, hData, 1);

			/* Start dragging now. */
			MyDragDropSource(pSrc); //blocking

			/* Free the source for the dragging now, as well as the two handles. */
			FreeMyDropSource(pSrc);
			GlobalFree(hData[0]);
		}
		else
		{
			object_error((t_object *)x, "failed to GlobalLock(hGlobal)");
		}
	}
	else
	{
		object_error((t_object *)x, "failed to allocate some memory for HGLOBAL");
	}


	sysmem_freeptr(files);
	return;
}

#else // Mac

void dragfiles_drag(t_dragfiles *x, t_symbol *s, long argc, t_atom *argv)
{
    int i;
    
    //early-exit-checks
    if (!argc) return;
    
    for (i = 0; i < argc; i++)
    {
        if (atom_gettype(argv + i) != A_SYM)
        {
            object_error((t_object *)s, "wrong input, please use a list of file path symbols");
            return;
        }
    }
    
    @autoreleasepool
    {
        
        //NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    
        NSMutableArray *paths = [[NSMutableArray alloc] initWithCapacity:argc];
        
        for (i = 0; i < argc; i++)
        {
            char thePath[MAX_PATH_CHARS];
            path_nameconform(atom_getsym(argv + i)->s_name, thePath, PATH_STYLE_NATIVE, PATH_TYPE_BOOT);
            
            //NSString* path = [[NSString stringWithUTF8String:argv[argc]] stringByExpandingTildeInPath];
            NSString* path = [[NSString stringWithUTF8String:thePath] stringByExpandingTildeInPath];
            
            if([path length] == 0)
            {
                continue;
            }
        
            //if(![path isAbsolutePath])
            //    path = [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:path];
        
            if(![[NSFileManager defaultManager] fileExistsAtPath:path])
            {
                object_error((t_object *)s, "The file “%s” does not exist. Ignoring.\n", [path UTF8String]);
                continue;
            }
        
            [paths addObject:path];
        }
        
        
        if([paths count] != 0)
        {
            // we have at least 1 valid file
            
            //check if the drag starts at a patcherview
            t_object *pvAt = NULL;
            NSView *cocoa_view = NULL;
            int X; int Y; double cx; double cy;
            jmouse_getposition_global(&X, &Y);
            pvAt = patcherview_findpatcherview(X, Y);
            
            if (pvAt)
            {
                //drag starts at a patcherview
                //post("drag starts at a patcherview");
                //get topview's native window
                pvAt = patcherview_get_topview(pvAt);
                object_method(pvAt, _sym_nativewindow, (void**)&cocoa_view);
                
               
                NSPoint mousePos = [[cocoa_view window] mouseLocationOutsideOfEventStream];
                cx = mousePos.x;
                cy = mousePos.y;
                
                if (cocoa_view)
                {
                    // send a mouseUp event to release the mousedown state of the patcher
                    NSPoint where;
                    where.x = cx;
                    where.y = cy;
                    //NSView* resp = [someView hitTest:where];
                    NSEventType evtType = NSLeftMouseUp;
                    NSEvent *mouseEvent = [NSEvent mouseEventWithType:evtType
                                                             location:where
                                                        modifierFlags:nil
                                                            timestamp:GetCurrentEventTime()
                                                         windowNumber:0
                                                              context:nil//[o->helper context]
                                                          eventNumber:nil
                                                           clickCount:1
                                                             pressure:0];
                    [cocoa_view mouseUp:mouseEvent];
                    
                    // now prepare a mousedown event to hold window and drag start position
                    evtType = NSLeftMouseDown;
                    NSEvent *mouseEvent2 = [NSEvent mouseEventWithType:evtType
                                                             location:where
                                                        modifierFlags:nil
                                                            timestamp:GetCurrentEventTime()
                                                         windowNumber:[[cocoa_view window] windowNumber]
                                                              context:nil//[o->helper context]
                                                          eventNumber:nil
                                                           clickCount:1
                                                             pressure:0];
                    
                    // now start the drag
                    NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
                    [pboard declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:nil];
                    [pboard setPropertyList:paths forType:NSFilenamesPboardType];
                    [[cocoa_view window] dragImage:[[NSWorkspace sharedWorkspace] iconForFiles:paths]
                                at:NSMakePoint(cx, cy)
                               offset:NSMakeSize(0, 0)
                                event:mouseEvent2
                           pasteboard:pboard
                               source:[[DragSource new] autorelease]
                            slideBack:NO];
                    
                    [paths release];
                    return; //success
                }
                else
                    object_error((t_object *)s, "error getting nativewindow");

            }
            else
            {
                //drag starts somewhere but not on a patcherwindow
                // create window to start drag from
                NSWindow* window = [[[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 0, 0)
                                                               styleMask:NSTitledWindowMask
                                                                 backing:NSBackingStoreBuffered
                                                                   defer:NO] autorelease];
                
                NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
                [pboard declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:nil];
                [pboard setPropertyList:paths forType:NSFilenamesPboardType];
                [window dragImage:[[NSWorkspace sharedWorkspace] iconForFiles:paths]
                               at:NSMakePoint(0, 0)
                           offset:NSMakeSize(0, 0)
                            event:nil
                       pasteboard:pboard
                           source:[[DragSource new] autorelease]
                        slideBack:NO];
                
                [paths release];
                return; //success
            }
        }
        else
            object_error((t_object *)s, "No valid file paths given.");
        
        
        [paths release];
        return;
        
    }
}
     



#endif



void dragfiles_assist(t_dragfiles *x, void *b, long m, long a, char *s)
{
	if (m == 1)
	{ 	// Inlets
		switch (a)
		{
		case 0: strcpy(s, ""); break;
		case 1: strcpy(s, ""); break;
		}
	}
}


void dragfiles_free(t_dragfiles *x)
{
	;
}


void *dragfiles_new(t_symbol *s, long argc, t_atom *argv)
{
	t_dragfiles *x = NULL;

    x = (t_dragfiles *)object_alloc(dragfiles_class);
	
	return (x);
}

