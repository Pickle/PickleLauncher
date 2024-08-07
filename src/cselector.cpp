/**
 *  @section LICENSE
 *
 *  PickleLauncher
 *  Copyright (C) 2010-2019 Scott Smith
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  @section LOCATION
 */

#include "cselector.h"

CSelector::CSelector() : CBase(),
        Redraw              (true),
        SkipFrame           (false),
        Rescan              (true),
        RefreshList         (true),
        SetOneEntryValue    (false),
        SetAllEntryValue    (false),
        TextScrollDir       (true),
        ExtractAllFiles     (false),
        DrawState_Title     (true),
        DrawState_About     (true),
        DrawState_Filter    (true),
        DrawState_FilePath  (true),
        DrawState_Index     (true),
        DrawState_ZipMode   (true),
        DrawState_Preview   (true),
        DrawState_ButtonL   (true),
        DrawState_ButtonR   (true),
        Mode                (MODE_SELECT_ENTRY),
        LastSelectedEntry   (0),
        TextScrollOffset    (0),
        CurScrollSpeed      (0),
        CurScrollPause      (0),
        ListNameHeight      (0),
        FramesDrawn         (0),
        FramesSkipped       (0),
        FramesSleep         (0),
#if defined(DEBUG)
        FPSDrawn            (0),
        FPSSkip             (0),
        FPSSleep            (0),
        FrameCountTime      (0),
        LoopTimeAverage     (0),
#endif
        FrameEndTime        (0),
        FrameStartTime      (0),
        FrameDelay          (0),
#if SDL_VERSION_ATLEAST(2,0,0)
        Window              (NULL),
        Renderer            (NULL),
        WindowBounds        (),
#endif
        Screen              (NULL),
        PixelFormat         (NULL),
        Mouse               (),
        Joystick            (NULL),
        ImageBackground     (NULL),
        ImagePointer        (NULL),
        ImageSelectPointer  (NULL),
        ImagePreview        (NULL),
        ImageTitle          (NULL),
        ImageAbout          (NULL),
        ImageFilePath       (NULL),
        ImageFilter         (NULL),
        ImageIndex          (NULL),
        ImageZipMode        (NULL),
#if defined(DEBUG)
        ImageDebug          (NULL),
#endif
        ImageButtons        (),
        Fonts               (),
        Config              (),
        Profile             (),
        System              (),
        ConfigPath          (DEF_CONFIG),
        ProfilePath         (DEF_PROFILE),
        ZipListPath         (DEF_ZIPLIST),
        EventReleased       (),
        EventPressCount     (),
        ButtonModesLeft     (),
        ButtonModesRight    (),
        DisplayList         (),
        LabelButtons        (),
        ListNames           (),
        ItemsEntry          (),
        ItemsArgument       (),
        ItemsValue          (),
        RectEntries         (),
        RectButtonsLeft     (),
        RectButtonsRight    (),
        ScreenRectsDirty    ()
{
    Fonts.resize( FONT_SIZE_TOTAL, NULL );

    ButtonModesLeft.resize( BUTTONS_MAX_LEFT );
    ButtonModesRight.resize( BUTTONS_MAX_RIGHT );
    RectButtonsLeft.resize( BUTTONS_MAX_LEFT );
    RectButtonsRight.resize( BUTTONS_MAX_RIGHT );
    ImageButtons.resize( EVENT_TOTAL, NULL );
    LabelButtons.resize( EVENT_TOTAL, "" );

    LabelButtons.at(EVENT_ONE_UP)       = BUTTON_LABEL_ONE_UP;
    LabelButtons.at(EVENT_ONE_DOWN)     = BUTTON_LABEL_ONE_DOWN;
    LabelButtons.at(EVENT_PAGE_UP)      = BUTTON_LABEL_PAGE_UP;
    LabelButtons.at(EVENT_PAGE_DOWN)    = BUTTON_LABEL_PAGE_DOWN;
    LabelButtons.at(EVENT_DIR_UP)       = BUTTON_LABEL_DIR_UP;
    LabelButtons.at(EVENT_DIR_DOWN)     = BUTTON_LABEL_DIR_DOWN;
    LabelButtons.at(EVENT_ZIP_MODE)     = BUTTON_LABEL_ZIP_MODE;
    LabelButtons.at(EVENT_CFG_APP)      = BUTTON_LABEL_CONFIG;
    LabelButtons.at(EVENT_CFG_ITEM)     = BUTTON_LABEL_EDIT;
    LabelButtons.at(EVENT_SET_ONE)      = BUTTON_LABEL_SET_ONE;
    LabelButtons.at(EVENT_SET_ALL)      = BUTTON_LABEL_SET_ALL;
    LabelButtons.at(EVENT_BACK)         = BUTTON_LABEL_BACK;
    LabelButtons.at(EVENT_SELECT)       = BUTTON_LABEL_SELECT;
    LabelButtons.at(EVENT_QUIT)         = BUTTON_LABEL_QUIT;

    DisplayList.resize( MODE_TOTAL );

    EventPressCount.resize( EVENT_TOTAL, EVENT_LOOPS_OFF );
    EventReleased.resize( EVENT_TOTAL, false );
}

CSelector::~CSelector()
{
}

int8_t CSelector::Run( int32_t argc, char** argv )
{
    int8_t result;

    result = 0;

    ProcessArguments( argc, argv );

    System.SetCPUClock( Config.CPUClock );

    // Load video,input,profile resources
    if (OpenResources())
    {
        result = 1;
    }

    // Display and poll the user for a selection
    if (result == 0)
    {
        int16_t selection = DisplayScreen();

        // Setup a exec script for execution following termination of this application
        if (selection >= 0)
        {
            if (RunExec( selection ))
            {
                result = 1;
            }
        }
        else if (selection < -1)
        {
            result = 1;
        }
        else
        {
            result = 0;
        }
    }

    // Release resources
    CloseResources( result );

    return result;
}

void CSelector::ProcessArguments( int argc, char** argv )
{
    string launcher;
    string argument;

    launcher = string(argv[0]);
    Profile.LauncherName = launcher.substr( launcher.find_last_of('/')+1 );
    Profile.LauncherPath = launcher.substr( 0, launcher.find_last_of('/')+1 );
    CheckPath( Profile.LauncherPath );

    if (Profile.LauncherPath.length() == 0)
    {
        Profile.LauncherPath = string(getenv("PWD"))+"/";
    }

#if defined(DEBUG)
    Log( __FILENAME__, __LINE__, "Running from '%s'", launcher.c_str() );
#endif
    Log( __FILENAME__, __LINE__, "Running from '%s' as '%s'", Profile.LauncherPath.c_str(), Profile.LauncherName.c_str() );

    for (uint8_t arg_index=0; arg_index<argc; arg_index++ )
    {
        argument = string(argv[arg_index]);

        if (argument.compare( ARG_RESETGUI ) == 0)
        {
            Config.ResetGUI = true;
        }
        else
        if (argument.compare( ARG_PROFILE ) == 0)
        {
            ProfilePath = string(argv[++arg_index]);
        }
        else
        if (argument.compare( ARG_CONFIG ) == 0)
        {
            ConfigPath = string(argv[++arg_index]);
        }
        else
        if (argument.compare( ARG_ZIPLIST ) == 0)
        {
            ZipListPath = string(argv[++arg_index]);
        }
    }
}

int8_t CSelector::OpenResources( void )
{
    uint32_t flags;
    string text;
    SDL_Surface* background = NULL;

    Log( __FILENAME__, __LINE__, "Loading config." );
    if (Config.Load( ConfigPath ))
    {
        Log( __FILENAME__, __LINE__, "Failed to load config" );
        return 1;
    }

    Log( __FILENAME__, __LINE__, "Loading ziplist." );
    if ((Config.UseZipSupport == true) && (Profile.Minizip.LoadUnzipList( ZipListPath )))
    {
        Log( __FILENAME__, __LINE__, "Failed to load ziplist" );
        return 1;
    }

    // Initialize defaults, Video and Audio subsystems
    Log( __FILENAME__, __LINE__, "Initializing SDL." );
    if (SDL_Init( SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK )==-1)
    {
        Log( __FILENAME__, __LINE__, "Failed to initialize SDL: %s.", SDL_GetError() );
        return 1;
    }
    Log( __FILENAME__, __LINE__, "SDL initialized." );

    // Setup SDL Screen
    flags = SCREEN_FLAGS;
    if (Config.Fullscreen == true)
    {
        flags |= FULLSCREEN;
    }

#if SDL_VERSION_ATLEAST(2,0,0)
    SDL_GetDisplayUsableBounds( 0, &WindowBounds );

    Window = SDL_CreateWindow("PickleLauncher", WindowBounds.x, WindowBounds.y,
                              Config.ScreenWidth, Config.ScreenHeight, flags);


#if defined(DEBUG)
    Log( __FILENAME__, __LINE__, "Window size %dx%d Bounds %dx%d", Config.ScreenWidth, Config.ScreenHeight, WindowBounds.x, WindowBounds.y );
#endif

    if (Window == NULL)
    {
        Log( __FILENAME__, __LINE__, "Failed to %dx%d video mode: %s", Config.ScreenWidth, Config.ScreenHeight, SDL_GetError() );
        return 1;
    }

    Screen = SDL_GetWindowSurface( Window );

    if (Screen == NULL)
    {
        Log( __FILENAME__, __LINE__, "Failed to get screen: %s", SDL_GetError() );
        return 1;
    }

    Renderer = SDL_CreateSoftwareRenderer( Screen );

    if (Renderer == NULL)
    {
        Log( __FILENAME__, __LINE__, "Failed to get renderer: %s", SDL_GetError() );
        return 1;
    }

    PixelFormat = SDL_AllocFormat( SDL_GetWindowPixelFormat(Window) );
#else /* SDL 1.2 */

    Screen = SDL_SetVideoMode( Config.ScreenWidth, Config.ScreenHeight, Config.ScreenDepth, flags );
    if (Screen == NULL)
    {
        Log( __FILENAME__, __LINE__, "Failed to %dx%dx%d video mode: %s", Config.ScreenWidth, Config.ScreenHeight, Config.ScreenDepth, SDL_GetError() );
        return 1;
    }

    PixelFormat = SDL_GetVideoSurface()->format;

    // Refresh entire screen for the first frame
    UpdateRect( 0, 0, Config.ScreenWidth, Config.ScreenHeight );
#endif

    // Load joystick
#if !defined(PANDORA) && !defined(X86)
    Log( __FILENAME__, __LINE__, "Joystick detected: %d", SDL_NumJoysticks() );
    Joystick = SDL_JoystickOpen(0);
    if (Joystick == NULL)
    {
        Log( __FILENAME__, __LINE__, "Warning failed to open first joystick: %s", SDL_GetError() );
    }
#endif

    // Setup TTF SDL
    if (TTF_Init() == -1)
    {
        Log( __FILENAME__, __LINE__, "Failed to init TTF_Init: %s", TTF_GetError() );
        return 1;
    }

    // Load ttf font
    Fonts.at(FONT_SIZE_SMALL) = TTF_OpenFont( Config.PathFont.c_str(), Config.FontSizes.at(FONT_SIZE_SMALL) );
    if (!Fonts.at(FONT_SIZE_SMALL))
    {
        Log( __FILENAME__, __LINE__, "Failed to open small TTF_OpenFont: %s", TTF_GetError() );
        return 1;
    }
    Fonts.at(FONT_SIZE_MEDIUM) = TTF_OpenFont( Config.PathFont.c_str(), Config.FontSizes.at(FONT_SIZE_MEDIUM) );
    if (!Fonts.at(FONT_SIZE_MEDIUM))
    {
        Log( __FILENAME__, __LINE__, "Failed to open medium TTF_OpenFont: %s", TTF_GetError() );
        return 1;
    }
    Fonts.at(FONT_SIZE_LARGE) = TTF_OpenFont( Config.PathFont.c_str(), Config.FontSizes.at(FONT_SIZE_LARGE) );
    if (!Fonts.at(FONT_SIZE_LARGE))
    {
        Log( __FILENAME__, __LINE__, "Failed to open large TTF_OpenFont: %s", TTF_GetError() );
        return 1;
    }

    Log( __FILENAME__, __LINE__, "Loading profile: %s", ProfilePath.c_str() );
    if (Profile.Load( ProfilePath, Config.Delimiter ))
    {
        Log( __FILENAME__, __LINE__, "Failed to load profile" );
        return 1;
    }

    // Load images
    background = LOAD_IMAGE( Config.PathBackground );
    if (background != NULL)
    {
        ImageBackground = ScaleSurface( background, Config.ScreenWidth, Config.ScreenHeight );
        FREE_IMAGE(background);
    }

    for (uint8_t button_index=0; button_index<Config.PathButtons.size(); button_index++)
    {
        ImageButtons.at(button_index) = LOAD_IMAGE( Config.PathButtons.at(button_index) );
    }

    //      Mouse pointer
    if (Config.ShowPointer==true)
    {
        ImagePointer = LOAD_IMAGE( Config.PathPointer );
        if (ImagePointer == NULL)
        {
            SDL_ShowCursor( SDL_ENABLE );
        }
        else
        {
            SDL_ShowCursor( SDL_DISABLE );
        }
    }
    else
    {
        SDL_ShowCursor( SDL_DISABLE );
    }

    //      List selector pointer
    ImageSelectPointer = LOAD_IMAGE( Config.PathSelectPointer );
    if (ImageSelectPointer == NULL)
    {
        ImageSelectPointer = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_MEDIUM), ENTRY_ARROW, Config.Colors.at(COLOR_BLACK) );
    }

    //      Title text
    text = string(APPNAME) + " " + string(APPVERSION);
    if (Profile.TargetApp.length() > 0)
    {
        text += " for " + Profile.TargetApp;
    }
    ImageTitle = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_LARGE), text.c_str(), Config.Colors.at(Config.ColorFontFiles) );
    if (ImageTitle == NULL)
    {
        Log( __FILENAME__, __LINE__, "Failed to create TTF surface with TTF_RenderText_Solid: %s", TTF_GetError() );
        return 1;
    }

    //      About text
    text = "Written by " + string(APPAUTHOR) + " " + string(APPCOPYRIGHT);
    ImageAbout = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_SMALL), text.c_str(), Config.Colors.at(Config.ColorFontFiles) );
    if (ImageAbout == NULL)
    {
        Log( __FILENAME__, __LINE__, "Failed to create TTF surface with TTF_RenderText_Solid: %s", TTF_GetError() );
        return 1;
    }

    return 0;
}

void CSelector::CloseResources( int8_t result )
{
    if (result == 0)
    {
        Config.Save( ConfigPath );
        Profile.Save( ProfilePath, Config.Delimiter );
    }

    if (Config.UseZipSupport == true)
    {
        Profile.Minizip.SaveUnzipList( ZipListPath );
    }

    // Close joystick
    if (Joystick != NULL)
    {
        Log( __FILENAME__, __LINE__, "Closing SDL Joystick." );
        SDL_JoystickClose( Joystick );
        Joystick = NULL;
    }

    // Close fonts
    Log( __FILENAME__, __LINE__, "Closing TTF fonts." );
    if (Fonts.at(FONT_SIZE_SMALL) != NULL)
    {
        TTF_CloseFont( Fonts.at(FONT_SIZE_SMALL) );
        Fonts.at(FONT_SIZE_SMALL) = NULL;
    }
    if (Fonts.at(FONT_SIZE_MEDIUM) != NULL)
    {
        TTF_CloseFont( Fonts.at(FONT_SIZE_MEDIUM) );
        Fonts.at(FONT_SIZE_MEDIUM) = NULL;
    }
    if (Fonts.at(FONT_SIZE_LARGE) != NULL)
    {
        TTF_CloseFont( Fonts.at(FONT_SIZE_LARGE) );
        Fonts.at(FONT_SIZE_LARGE) = NULL;
    }

    // Free images
    FREE_IMAGE( ImageBackground );
    FREE_IMAGE( ImagePointer );
    FREE_IMAGE( ImageSelectPointer );
    FREE_IMAGE( ImagePreview );
    FREE_IMAGE( ImageTitle );
    FREE_IMAGE( ImageAbout );
    FREE_IMAGE( ImageFilePath );
    FREE_IMAGE( ImageFilter );
    FREE_IMAGE( ImageIndex );
    FREE_IMAGE( ImageZipMode );
#if defined(DEBUG)
    FREE_IMAGE( ImageDebug );
#endif
    for (uint8_t button_index=0; button_index<ImageButtons.size(); button_index++)
    {
        FREE_IMAGE( ImageButtons.at(button_index) );
    }

#if SDL_VERSION_ATLEAST(2,0,0)
    if (PixelFormat != NULL)
    {
        SDL_FreeFormat( PixelFormat );
    }

    if (Renderer != NULL)
    {
        SDL_DestroyRenderer( Renderer );
    }

    if (Window != NULL)
    {
        SDL_DestroyWindow( Window );
    }
#endif

    Log( __FILENAME__, __LINE__, "Quitting TTF." );
    TTF_Quit();

    Log( __FILENAME__, __LINE__, "Quitting SDL." );
    SDL_Quit();

    // Flush all std buffers before exit
    fflush( stdout );
    fflush( stderr );
}

int16_t CSelector::DisplayScreen( void )
{
    while (   (IsEventOff(EVENT_QUIT) == true)
           && (   (IsEventOff(EVENT_SELECT) == true)
               || (Mode != MODE_SELECT_ENTRY)
              )
          )
    {
        // Get user input
        if (PollInputs())
        {
            return -2;
        }

        // Select the mode
        SelectMode();

        // Configure the buttons according to the mode
        if (ConfigureButtons())
        {
            return -2;
        }

        // Draw the selector
        if (DisplaySelector())
        {
            return -2;
        }

        // Update the screen
        UpdateScreen();
    }

    if (IsEventOn( EVENT_QUIT ) == true)
    {
        // Detete any files exracted from zip
        Profile.Minizip.DelUnzipFiles();

        return -1;
    }
    else
    {
        return DisplayList.at(MODE_SELECT_ENTRY).absolute;
    }
}

void CSelector::UpdateRect( int16_t x, int16_t y, int16_t w, int16_t h )
{
    SDL_Rect rect;

    if (Config.ScreenFlip == false)
    {
        // Safety Checks
        if( x < 0 )
        {
            x = 0;
            Log( __FILENAME__, __LINE__, "ERROR: UpdateRect X was out of bounds" );
        }

        if( y < 0 )
        {
            y = 0;
            Log( __FILENAME__, __LINE__, "ERROR: UpdateRect Y was out of bounds" );
        }

        if( h < 0 )
        {
            h = 0;
            Log( __FILENAME__, __LINE__, "ERROR: UpdateRect X was out of bounds" );
        }

        if( w < 0 )
        {
            w = 0;
            Log( __FILENAME__, __LINE__, "ERROR: UpdateRect Y was out of bounds" );
        }

        if( x > Config.ScreenWidth )
        {
            x = Config.ScreenWidth-1;
            Log( __FILENAME__, __LINE__, "ERROR: UpdateRect X was out of bounds" );
        }

        if( y > Config.ScreenHeight )
        {
            y = Config.ScreenHeight-1;
            Log( __FILENAME__, __LINE__, "ERROR: UpdateRect Y was out of bounds" );
        }

        if( x + w > Config.ScreenWidth )
        {
            w = Config.ScreenWidth-x;
            Log( __FILENAME__, __LINE__, "ERROR: UpdateRect W was out of bounds" );
        }

        if( y + h > Config.ScreenHeight )
        {
            h = Config.ScreenHeight-y;
            Log( __FILENAME__, __LINE__, "ERROR: UpdateRect H was out of bounds" );
        }

        rect.x = x;
        rect.y = y;
        rect.w = w;
        rect.h = h;

        ScreenRectsDirty.push_back( rect );
    }
}

void CSelector::UpdateScreen( void )
{
#if SDL_VERSION_ATLEAST(2,0,0)
    SDL_UpdateWindowSurface( Window );

    FramesDrawn++;
#else /* SDL 1.2 */
#if defined(DEBUG_FORCE_REDRAW)
    Redraw = true;
#endif

    if ((SkipFrame == false) && (Redraw == true))
    {
        if (Config.ScreenFlip == true)
        {

            if (SDL_Flip( Screen ) != 0)
            {
                Log( __FILENAME__, __LINE__, "Failed to swap the buffers: %s", SDL_GetError() );
            }
        }
        else
        {
            SDL_UpdateRects( Screen, ScreenRectsDirty.size(), &ScreenRectsDirty[0] );
        }

        Redraw = false;
        FramesDrawn++;
    }
    else
    {
        if (SkipFrame == true)
        {
            FramesSkipped++;
        }
        else
        {
            FramesSleep++;
        }
    }
    ScreenRectsDirty.clear();
#endif

    FrameEndTime = SDL_GetTicks();
    FrameDelay   = (MS_PER_SEC/FRAMES_PER_SEC) - (FrameEndTime - FrameStartTime);

#if defined(DEBUG_FPS)
    LoopTimeAverage = (LoopTimeAverage + (FrameEndTime - FrameStartTime))/2;
#endif

    if (FrameDelay < 0)
    {
        if (FramesSkipped/FramesDrawn < FRAME_SKIP_RATIO)
        {
            SkipFrame = true;
        }
        else // Force a frame to be drawn
        {
            SkipFrame = false;
        }
    }
    else
    {
        SkipFrame = false;
        SDL_Delay( MIN(FrameDelay, MS_PER_SEC) );
    }
    FrameStartTime = SDL_GetTicks();

#if defined(DEBUG_FPS)
    if (FrameStartTime - FrameCountTime >= MS_PER_SEC)
    {
        FrameCountTime  = FrameStartTime;
        FPSDrawn        = FramesDrawn;
        FPSSkip         = FramesSkipped;
        FPSSleep        = FramesSleep;
        FramesDrawn     = 1;
        FramesSkipped   = 0;
        FramesSleep     = 0;

        cout << "DEBUG total " << i_to_a(FPSDrawn+FPSSkip+FPSSleep)
             << " fps " << i_to_a(FPSDrawn) << " skip " << i_to_a(FPSSkip) << " slp " << i_to_a(FPSSleep)
             << " loop " << i_to_a(LoopTimeAverage) << endl;
    }
#endif
}

void CSelector::SelectMode( void )
{
    uint8_t old_mode;

    old_mode = Mode;

    switch (Mode)
    {
        case MODE_SELECT_ENTRY:
            if (IsEventOn( EVENT_CFG_ITEM ) == true)
            {
                if (ItemsEntry.size()>0)
                {
                    if (   (ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Type == TYPE_FILE)
                        || (   (ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Type == TYPE_DIR)
                            && (Profile.LaunchableDirs == true)
                           )
                       )
                    {
                        Mode = MODE_SELECT_ARGUMENT;
                    }
                }
            }
            else if (IsEventOn( EVENT_CFG_APP ) == true)
            {
                Mode = MODE_SELECT_OPTION;
            }
            break;
        case MODE_SELECT_ARGUMENT:
            if (IsEventOn( EVENT_BACK ) == true)
            {
                Mode = MODE_SELECT_ENTRY;
            }
            if (IsEventOn( EVENT_SELECT ) == true)
            {
                Mode = MODE_SELECT_VALUE;
            }
            break;
        case MODE_SELECT_VALUE:
            if (IsEventOn( EVENT_BACK ) == true)
            {
                Mode = MODE_SELECT_ARGUMENT;
            }
            break;
        case MODE_SELECT_OPTION:
            if (IsEventOn( EVENT_BACK ) == true)
            {
                Mode = MODE_SELECT_ENTRY;
            }
            if (IsEventOn( EVENT_SELECT ) == true)
            {
                Mode = MODE_SELECT_VALUE;
            }
            break;
        default:
            Mode = MODE_SELECT_ENTRY;
            Log( __FILENAME__, __LINE__, "Error: Unknown Mode" );
            break;
    }

    if (Mode != old_mode)
    {
        DrawState_ButtonL  = true;
        DrawState_ButtonR  = true;
        Rescan = true;
    }
}

int8_t CSelector::DisplaySelector( void )
{
    SDL_Rect rect_pos = { Config.EntryXOffset, Config.EntryYOffset, 0 ,0 };

    if (Rescan)
    {
        RescanItems();
        RefreshList     = true;
        Rescan          = false;
    }

    if (RefreshList)
    {
        PopulateList();
        DrawState_Index = true;
        Redraw          = true;
        RefreshList     = false;
    }

    if ((Redraw == true) || (CurScrollPause != 0) || (CurScrollSpeed != 0) || (TextScrollOffset != 0))
    {
        if (Config.ScreenFlip == true)
        {
            DrawState_Title     = true;
            DrawState_About     = true;
            DrawState_Filter    = true;
            DrawState_FilePath  = true;
            DrawState_Index     = true;
            DrawState_ZipMode   = true;
            DrawState_Preview   = true;
            DrawState_ButtonL   = true;
            DrawState_ButtonR   = true;
        }
#if defined(DEBUG_DRAW_STATES)
        else
        {
            cout << "DEBUG "
                 << " " << i_to_a(DrawState_Title)
                 << " " << i_to_a(DrawState_About)
                 << " " << i_to_a(DrawState_Filter)
                 << " " << i_to_a(DrawState_FilePath)
                 << " " << i_to_a(DrawState_Index)
                 << " " << i_to_a(DrawState_ZipMode)
                 << " " << i_to_a(DrawState_Preview)
                 << " " << i_to_a(DrawState_ButtonL)
                 << " " << i_to_a(DrawState_ButtonR) << endl;
        }
#endif

        // Draw background or clear screen
        DrawBackground();

        // Draw text titles to the screen
        if (DrawText( rect_pos ))
        {
            return 1;
        }

        // Draw the buttons for touchscreen
        if (DrawButtons( rect_pos ))
        {
            return 1;
        }

        // Draw the names for the items for display
        if (DrawNames( rect_pos ))
        {
            return 1;
        }

        // Custom mouse pointer
        if ((Config.ShowPointer == true) && (ImagePointer != NULL))
        {
            ApplyImage( Mouse.x, Mouse.y, ImagePointer, Screen, NULL );
        }
    }

    return 0;
}

void CSelector::DirectoryUp( void )
{
    if (Profile.FilePath.length() > 0)
    {
        if (Profile.FilePath.at( Profile.FilePath.length()-1) == '/')
        {
            Profile.FilePath.erase( Profile.FilePath.length()-1 );
        }
        Profile.FilePath = Profile.FilePath.substr( 0, Profile.FilePath.find_last_of('/', Profile.FilePath.length()-1) ) + '/';
        DrawState_FilePath  = true;
        Rescan              = true;
    }
    else
    {
        Log( __FILENAME__, __LINE__, "Error: Filepath is empty" );
    }
}

void CSelector::DirectoryDown( void )
{
    if (DisplayList.at(MODE_SELECT_ENTRY).absolute < (int16_t)ItemsEntry.size() )
    {
        if (ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Type == TYPE_DIR )
        {
            Profile.FilePath += ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Name + '/';
            DrawState_FilePath  = true;
            Rescan              = true;

            EventPressCount.at(EVENT_SELECT) = EVENT_LOOPS_OFF;
        }
    }
    else
    {
        Log( __FILENAME__, __LINE__, "Error: Item index of %d too large for size of scanitems %d", DisplayList.at(MODE_SELECT_ENTRY).absolute, ItemsEntry.size() );
    }
}

void CSelector::ZipUp( void )
{
    DrawState_FilePath  = true;
    DrawState_ZipMode   = true;
    DrawState_ButtonL   = true;
    Rescan              = true;
    Profile.ZipFile     = "";
}

void CSelector::ZipDown( void )
{
    DrawState_FilePath  = true;
    DrawState_ZipMode   = true;
    DrawState_ButtonL   = true;
    Rescan              = true;
    Profile.ZipFile = ItemsEntry.at(DisplayList.at(Mode).absolute).Name;
    EventPressCount.at( EVENT_SELECT ) = EVENT_LOOPS_OFF;
}

void CSelector::RescanItems( void )
{
    uint16_t total;

    switch (Mode)
    {
        case MODE_SELECT_ENTRY:
            Profile.ScanDir( Profile.FilePath, Config.ShowHidden, Config.UseZipSupport, ItemsEntry );
            total = ItemsEntry.size();
            break;
        case MODE_SELECT_ARGUMENT:
            Profile.ScanEntry( ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute), ItemsArgument );
            total = ItemsArgument.size();
            break;
        case MODE_SELECT_VALUE:
            Profile.ScanArgument( ItemsArgument.at(DisplayList.at(MODE_SELECT_ARGUMENT).absolute), ItemsValue );
            total = ItemsValue.size();
            break;
        case MODE_SELECT_OPTION:
            //Profile.ScanOptions( ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute), ItemsArgument );
            total = ItemsArgument.size();
            break;
        default:
            total = 0;
            Log( __FILENAME__, __LINE__, "Error: Unknown Mode" );
            break;
    }

    if (total > Config.MaxEntries)
    {
        RectEntries.resize( Config.MaxEntries );
    }
    else
    {
        RectEntries.resize( total );
    }
    ListNames.resize( RectEntries.size() );

    if (Mode == MODE_SELECT_ENTRY)
    {
        DisplayList.at(Mode).absolute = Config.PrevEntryIndex;
        if (DisplayList.at(Mode).absolute < MAX_ENTRIES)
        {
            DisplayList.at(Mode).relative = DisplayList.at(Mode).absolute;
        }
        else /* Multiple pages of entries */
        {
            /* If the last selected item is in the last page go back some entries to make full page */
            if (DisplayList.at(Mode).absolute > total-MAX_ENTRIES)
            {
                DisplayList.at(Mode).first    = total-MAX_ENTRIES;
                DisplayList.at(Mode).relative = DisplayList.at(Mode).absolute - DisplayList.at(Mode).first;
            }
            else
            {
                DisplayList.at(Mode).first    = DisplayList.at(Mode).absolute;
                DisplayList.at(Mode).relative = 0;
            }
        }
    }
    else
    {
        DisplayList.at(Mode).absolute = 0;
        DisplayList.at(Mode).relative = 0;
        DisplayList.at(Mode).first    = 0;
    }

    DisplayList.at(Mode).last      = MIN( DisplayList.at(Mode).first+MAX_ENTRIES, total-1 );
    DisplayList.at(Mode).total     = total;
}

void CSelector::PopulateList( void )
{
    // Set limits
    SelectionLimits( DisplayList.at( Mode ) );

    switch (Mode)
    {
        case MODE_SELECT_ENTRY:
            PopModeEntry();
            break;
        case MODE_SELECT_ARGUMENT:
            PopModeArgument();
            break;
        case MODE_SELECT_VALUE:
            PopModeValue();
            break;
        case MODE_SELECT_OPTION:
            PopModeOption();
            break;
        default:
            Log( __FILENAME__, __LINE__, "Error: CSelector::PopulateList Unknown Mode" );
            break;
    }
}

void CSelector::PopModeEntry( void )
{
    for (uint16_t i=0; i<ListNames.size(); i++)
    {
        uint16_t index = DisplayList.at( MODE_SELECT_ENTRY ).first+i;

        if (CheckRange( index, ItemsEntry.size() ))
        {
            ListNames.at(i).text.clear();
            if (ItemsEntry.at(index).Entry >= 0)
            {
                ListNames.at(i).text = Profile.Entries.at(ItemsEntry.at(index).Entry).Alias;
            }
            if (ListNames.at(i).text.length() == 0)
            {
                ListNames.at(i).text = ItemsEntry.at(index).Name;
            }

            if (Config.ShowExts == false)
            {
                ListNames.at(i).text = ListNames.at(i).text.substr( 0, ListNames.at(i).text.find_last_of(".") );
            }
            ListNames.at(i).text = ListNames.at(i).text;

            if (index == DisplayList.at(Mode).absolute)
            {
                ListNames.at(i).font = FONT_SIZE_LARGE;
                LoadPreview( ListNames.at(i).text );        // Load preview
            }
            else
            {
                ListNames.at(i).font = FONT_SIZE_MEDIUM;
            }

            ListNames.at(i).color = Config.ColorFontFiles;
            if (ItemsEntry.at(index).Type == TYPE_DIR)
            {
                ListNames.at(i).color = Config.ColorFontFolders;
            }
        }
        else
        {
            Log( __FILENAME__, __LINE__, "Error: CSelector::PopulateModeSelectEntry Index Error" );
        }
    }

    Config.PrevEntryIndex = DisplayList.at( MODE_SELECT_ENTRY ).absolute;
}

void CSelector::PopModeArgument( void )
{
    for (uint16_t i=0; i<ListNames.size(); i++)
    {
        uint16_t index = DisplayList.at(MODE_SELECT_ARGUMENT).first+i;

        if (CheckRange( index, ItemsArgument.size() ))
        {
            ListNames.at(i).text = ItemsArgument.at(index).Name;

            if (i == DisplayList.at(MODE_SELECT_ARGUMENT).absolute)
            {
                ListNames.at(i).font = FONT_SIZE_LARGE;
            }
            else
            {
                ListNames.at(i).font = FONT_SIZE_MEDIUM;
            }

            ListNames.at(i).color = Config.ColorFontFiles;
        }
        else
        {
            Log( __FILENAME__, __LINE__, "Error: PopModeArgument index is out of range" );
        }
    }
}

void CSelector::PopModeValue( void )
{
    listoption_t argument;

    if (CheckRange(DisplayList.at(MODE_SELECT_ARGUMENT).absolute, ItemsArgument.size() ))
    {
        argument = ItemsArgument.at(DisplayList.at(MODE_SELECT_ARGUMENT).absolute);

        for (uint16_t i=0; i<ListNames.size(); i++)
        {
            uint16_t index = DisplayList.at(MODE_SELECT_VALUE).first+i;

            if (CheckRange( index, ItemsValue.size() ))
            {
                ListNames.at(i).text = ItemsValue.at(index);

                if (index == DisplayList.at(MODE_SELECT_VALUE).absolute)
                {
                    ListNames.at(i).font = FONT_SIZE_LARGE;
                }
                else
                {
                    ListNames.at(i).font = FONT_SIZE_MEDIUM;
                }

                // Detect the default value
                int16_t defvalue;

                if (ItemsArgument.at(DisplayList.at(MODE_SELECT_ARGUMENT).absolute).Command >= 0)
                {
                    if (SetAllEntryValue == true)
                    {
                        Profile.Commands.at(argument.Command).Arguments.at(argument.Argument).Default = DisplayList.at(MODE_SELECT_VALUE).absolute;
                    }
                    defvalue = Profile.Commands.at(argument.Command).Arguments.at(argument.Argument).Default;
                }
                else
                {
                    if (SetAllEntryValue == true)
                    {
                        Profile.Extensions.at(argument.Extension).Arguments.at(argument.Argument).Default = DisplayList.at(MODE_SELECT_VALUE).absolute;
                    }
                    defvalue = Profile.Extensions.at(argument.Extension).Arguments.at(argument.Argument).Default;
                }

                if (index == defvalue)
                {
                    ListNames.at(i).text += ENTRY_DEFAULT_VALUE;
                }

                // Set the color for the selected item for the entry
                ListNames.at(i).color = Config.ColorFontFiles;
                if (ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry < 0)
                {
                    // A custom value has been selected, so create a new entry
                    if (SetOneEntryValue == true)
                    {
                        int16_t entry = Profile.AddEntry( argument, ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Name );

                        if (entry > 0)
                        {
                            ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry = entry;
                        }
                        else
                        {
                            Log( __FILENAME__, __LINE__, "Error: Could not create new entry" );
                        }
                    }
                    else if (index == defvalue)
                    {
                        ListNames.at(i).color = COLOR_RED;
                    }
                }

                if (CheckRange( ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry, ItemsEntry.size()))
                {
                    entry_t* profile_entry = &Profile.Entries.at( ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry );

                    if (ItemsArgument.at(DisplayList.at(MODE_SELECT_ARGUMENT).absolute).Command >= 0)
                    {
                        /* Set the command value if one is selected */
                        if ((SetOneEntryValue == true) || (SetAllEntryValue == true))
                        {
                            if (DisplayList.at(MODE_SELECT_VALUE).absolute != defvalue)
                            {
                                profile_entry->CmdValues.at(argument.Command+argument.Argument) = DisplayList.at(MODE_SELECT_VALUE).absolute;
                            }
                            else
                            {
                                profile_entry->CmdValues.at(argument.Command+argument.Argument) = DEFAULT_VALUE;
                            }
                        }

                        /* Highlight the item with red for the value that will be used for this entry */
                        if (   (   (index == defvalue)
                                && (profile_entry->CmdValues.at(argument.Command+argument.Argument) == DEFAULT_VALUE)
                               )
                            || (index == profile_entry->CmdValues.at(argument.Command+argument.Argument))
                           )
                        {
                            ListNames.at(i).color = COLOR_RED;
                        }
                    }
                    else
                    {
                        /* Set the command value if one is selected */
                        if ((SetOneEntryValue == true) || (SetAllEntryValue == true))
                        {
                            if (DisplayList.at(MODE_SELECT_VALUE).absolute != defvalue)
                            {
                                profile_entry->ArgValues.at(argument.Argument) = DisplayList.at(MODE_SELECT_VALUE).absolute;
                            }
                            else
                            {
                                profile_entry->ArgValues.at(argument.Argument) = DEFAULT_VALUE;
                            }
                        }

                        /* Highlight the item with red for the value that will be used for this entry */
                        if (   (   (index == defvalue)
                                && (profile_entry->ArgValues.at(argument.Argument) == DEFAULT_VALUE)
                               )
                            || (index == profile_entry->ArgValues.at(argument.Argument))
                           )
                        {
                            ListNames.at(i).color = COLOR_RED;
                        }
                    }
                }
            }
            else
            {
                Log( __FILENAME__, __LINE__, "Error: PopModeValue index is out of range" );
            }
        }
    }
    else
    {
        Log( __FILENAME__, __LINE__, "Error: PopModeValue argument index out of range" );
    }
}

void CSelector::PopModeOption( void )
{
    // TODO
}

void CSelector::LoadPreview( const string& name )
{
    string filename;
    SDL_Surface* preview = NULL;

    if (ImagePreview != NULL)
    {
        DrawState_Preview = true;
    }

    FREE_IMAGE( ImagePreview );

    filename = Config.PreviewsPath + "/" + name.substr( 0, name.find_last_of(".")) + ".png";

#if defined(DEBUG)
    Log( __FILENAME__, __LINE__, "Loading preview picture: %s", filename.c_str() );
#endif

    preview = LOAD_IMAGE( filename );
    if (preview != NULL)
    {
        ImagePreview = ScaleSurface( preview, Config.PreviewWidth, Config.PreviewHeight );
        FREE_IMAGE( preview );
        DrawState_Preview = true;
    }
}

int8_t CSelector::DrawNames( SDL_Rect& location )
{
    uint16_t startx, starty;
    uint16_t entry_height = 0;
    SDL_Rect rect_clip;
    SDL_Surface* text_surface = NULL;

    if (Config.AutoLayout == false)
    {
        location.x = Config.PosX_ListNames;
        location.y = Config.PosY_ListNames;
    }

    startx = location.x;
    starty = location.y;

    if (ListNames.size() == 0)
    {
        // Empty directories or zip files
        if ((Config.UseZipSupport == true) && (Profile.ZipFile.length() > 0))
        {
            text_surface = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_MEDIUM), EMPTY_ZIP_LABEL, Config.Colors.at(COLOR_BLACK) );
        }
        else
        {
            text_surface = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_MEDIUM), EMPTY_DIR_LABEL, Config.Colors.at(COLOR_BLACK) );
        }

        if (text_surface != NULL)
        {
            location.x += ImageSelectPointer->w;

            rect_clip.x = 0;
            rect_clip.y = 0;
            rect_clip.w = Config.DisplayListMaxWidth-location.x;
            rect_clip.h = text_surface->h;

            ApplyImage( location.x, location.y, text_surface, Screen, &rect_clip );

            ListNameHeight = MAX(ListNameHeight, location.y+text_surface->h );
            location.x -= ImageSelectPointer->w;
            location.y += text_surface->h + Config.EntryYDelta;

            FREE_IMAGE( text_surface );
        }
        else
        {
            Log( __FILENAME__, __LINE__, "Failed to create TTF surface with TTF_RenderText_Solid: %s", TTF_GetError() );
            return 1;
        }
    }
    else
    {
        for (uint16_t entry_index=0; entry_index<ListNames.size(); entry_index++)
        {
            if (ListNames.at(entry_index).text.length() > 0)
            {
                text_surface = TTF_RenderText_Solid( Fonts.at(ListNames.at(entry_index).font), ListNames.at(entry_index).text.c_str(), Config.Colors.at(ListNames.at(entry_index).color) );

                if (text_surface != NULL)
                {
                    int16_t offset = 0;

                    location.x += (ImageSelectPointer->w + POINTER_OFFSET);
                    RectEntries.at(entry_index).x = location.x;
                    RectEntries.at(entry_index).y = location.y;
                    RectEntries.at(entry_index).w = text_surface->w;
                    RectEntries.at(entry_index).h = text_surface->h;

                    if (text_surface->w > (Config.DisplayListMaxWidth-location.x) )
                    {
                        RectEntries.at(entry_index).w = Config.DisplayListMaxWidth-location.x;

                        if ((Config.TextScrollOption == true) && (DisplayList.at(Mode).relative == entry_index))
                        {
                            offset = TextScrollOffset;

                            if (CurScrollPause > 1)
                            {
                                CurScrollPause++;
                                if (CurScrollPause >= Config.ScrollPauseSpeed)
                                {
                                    CurScrollPause = 1;
                                }
                            }
                            else
                            {
                                CurScrollSpeed++;
                                if (CurScrollSpeed >= Config.ScrollSpeed)
                                {
                                    CurScrollSpeed = 1;
                                    if (TextScrollDir == true)
                                    {
                                        TextScrollOffset += (int16_t)Config.ScreenRatioW;
                                    }
                                    else
                                    {
                                        TextScrollOffset -= (int16_t)Config.ScreenRatioW;
                                    }
                                    Redraw = true;
                                }

                                if (RectEntries.at(entry_index).w+TextScrollOffset >= text_surface->w)
                                {
                                    TextScrollDir   = false;
                                    CurScrollPause  = 2;
                                }
                                else if (TextScrollOffset == 0)
                                {
                                    TextScrollDir   = true;
                                    CurScrollPause  = 2;
                                }
                            }
                        }
                    }

                    rect_clip.w = Config.DisplayListMaxWidth-location.x;
                    rect_clip.h = text_surface->h;
                    rect_clip.x = offset;
                    rect_clip.y = 0;

                    ApplyImage( location.x, location.y,  text_surface, Screen, &rect_clip );

                    ListNameHeight = MAX(ListNameHeight, location.y+text_surface->h );
                    location.x -= (ImageSelectPointer->w + POINTER_OFFSET);

                    entry_height = text_surface->h;

                    FREE_IMAGE( text_surface );
                }
                else
                {
                    Log( __FILENAME__, __LINE__, "Failed to create TTF surface with TTF_RenderText_Solid: %s", TTF_GetError() );
                    return 1;
                }
            }

            // Draw the selector pointer
            if (entry_index == DisplayList.at(Mode).relative)
            {
                int16_t offset = MAX( 0, (entry_height-ImageSelectPointer->h)/2 );

                ApplyImage( location.x, location.y + offset, ImageSelectPointer, Screen, NULL );

                // Reset scroll settings
                if (entry_index != LastSelectedEntry)
                {
                    CurScrollPause      = 0;
                    CurScrollSpeed      = 0;
                    TextScrollOffset    = 0;
                    TextScrollDir       = true;
                }
                LastSelectedEntry = entry_index;
            }

            location.y += entry_height + Config.EntryYDelta;
        }
    }

    UpdateRect( startx, starty, Config.DisplayListMaxWidth-startx, ListNameHeight-starty );

    return 0;
}

void CSelector::SelectionLimits( item_pos_t& pos )
{
    if (pos.absolute <= pos.first)
    {
        if (pos.relative < 0)
        {
            pos.relative = 0;
        }

        if (pos.absolute < 0)
        {
            pos.absolute = 0;
        }

        pos.first = pos.absolute;
        if (pos.total < Config.MaxEntries)
        {
            pos.last = (pos.total-1);
        }
        else
        {
            pos.last = pos.absolute+(Config.MaxEntries-1);
        }
    }
    else if (pos.absolute >= pos.last)
    {
        if (pos.absolute > (int16_t)(pos.total-1))
        {
            pos.absolute = (pos.total-1);
        }

        pos.first   = pos.absolute-(Config.MaxEntries-1);
        pos.last    = pos.absolute;
        if (pos.total < Config.MaxEntries)
        {
            pos.relative = (pos.total-1);
        }
        else
        {
            pos.relative = Config.MaxEntries-1;
        }

        if (pos.first < 0)
        {
            pos.first = 0;
        }
    }
}

void CSelector::DrawBackground( void )
{
    if (ImageBackground != NULL)
    {
        ApplyImage( 0, 0, ImageBackground, Screen, NULL );
    }
    else
    {
        SDL_FillRect( Screen, NULL, rgb_to_int(Config.Colors.at(Config.ColorBackground), PixelFormat) );
    }
}

int8_t CSelector::ConfigureButtons( void )
{
    // Common button mappings
    ButtonModesLeft.at(0) = EVENT_ONE_UP;
    ButtonModesLeft.at(1) = EVENT_PAGE_UP;
    ButtonModesLeft.at(2) = EVENT_PAGE_DOWN;
    ButtonModesLeft.at(3) = EVENT_ONE_DOWN;

    ButtonModesRight.at(0) = EVENT_QUIT;

    // Specific button mappings
    switch (Mode)
    {
        case MODE_SELECT_ENTRY:
            ButtonModesLeft.at(4) = EVENT_DIR_UP;
            ButtonModesLeft.at(5) = EVENT_DIR_DOWN;
            if ((Config.UseZipSupport == true) && (Profile.ZipFile.length() > 0))
            {
                ButtonModesLeft.at(6) = EVENT_ZIP_MODE;
            }
            else
            {
                ButtonModesLeft.at(6) = EVENT_NONE;
            }

            ButtonModesRight.at(1) = EVENT_SELECT;
            ButtonModesRight.at(2) = EVENT_CFG_ITEM;
            ButtonModesRight.at(3) = EVENT_NONE;
            //ButtonModesRight.at(3) = EVENT_CFG_APP;   // TODO
            break;
        case MODE_SELECT_ARGUMENT:
            ButtonModesLeft.at(4) = EVENT_NONE;
            ButtonModesLeft.at(5) = EVENT_NONE;
            ButtonModesLeft.at(6) = EVENT_NONE;

            ButtonModesRight.at(1) = EVENT_BACK;
            ButtonModesRight.at(2) = EVENT_SELECT;
            ButtonModesRight.at(3) = EVENT_NONE;
            break;
        case MODE_SELECT_VALUE:
            ButtonModesLeft.at(4) = EVENT_NONE;
            ButtonModesLeft.at(5) = EVENT_NONE;
            ButtonModesLeft.at(6) = EVENT_NONE;

            ButtonModesRight.at(1) = EVENT_BACK;
            ButtonModesRight.at(2) = EVENT_SET_ALL;
            ButtonModesRight.at(3) = EVENT_SET_ONE;
            break;
        case MODE_SELECT_OPTION:
            ButtonModesLeft.at(4) = EVENT_NONE;
            ButtonModesLeft.at(5) = EVENT_NONE;
            ButtonModesLeft.at(6) = EVENT_NONE;

            ButtonModesRight.at(1) = EVENT_BACK;
            ButtonModesRight.at(2) = EVENT_SELECT;
            ButtonModesRight.at(3) = EVENT_NONE;
            break;
        default:
            Log( __FILENAME__, __LINE__, "Error: Unknown Mode" );
            return 1;
            break;
    }

    // Overides for button driven by config options
    for (uint16_t i=0; i<ButtonModesLeft.size(); i++ )
    {
        if (Config.ButtonModesLeftEnable.at(i) == false)
        {
            ButtonModesLeft.at(i) = EVENT_NONE;
        }
    }

    for (uint16_t i=0; i<ButtonModesRight.size(); i++ )
    {
        if (Config.ButtonModesRightEnable.at(i) == false)
        {
            ButtonModesRight.at(i) = EVENT_NONE;
        }
    }

    return 0;
}

int8_t CSelector::DrawButtons( SDL_Rect& location )
{
    SDL_Rect preview;

    if (Config.AutoLayout == false)
    {
        location.x = Config.PosX_ButtonLeft;
        location.y = Config.PosY_ButtonLeft;
    }

    // Draw buttons on left
    if (DrawState_ButtonL == true)
    {
        for (uint8_t button=0; button<BUTTONS_MAX_LEFT; button++)
        {
            RectButtonsLeft.at(button).x = location.x;
            RectButtonsLeft.at(button).y = location.y+(Config.ButtonHeightLeft*button)+(Config.EntryYOffset*button);
            RectButtonsLeft.at(button).w = Config.ButtonWidthLeft;
            RectButtonsLeft.at(button).h = Config.ButtonHeightLeft;

            if (ButtonModesLeft.at(button) != EVENT_NONE)
            {
                if (DrawButton( ButtonModesLeft.at(button), Fonts.at(FONT_SIZE_LARGE), RectButtonsLeft.at(button) ))
                {
                    return 1;
                }
            }
            UpdateRect( RectButtonsLeft.at(button).x, RectButtonsLeft.at(button).y, RectButtonsLeft.at(button).w, RectButtonsLeft.at(button).h );
        }
        DrawState_ButtonL = false;
    }
    location.x += Config.ButtonWidthLeft + Config.EntryXOffset;

    // Draw buttons on right
    if (DrawState_ButtonR == true)
    {
        for (uint8_t button=0; button<BUTTONS_MAX_RIGHT; button++)
        {
            if (Config.AutoLayout == false)
            {
                RectButtonsRight.at(button).x = Config.PosX_ButtonRight;
                RectButtonsRight.at(button).y = Config.PosY_ButtonRight-(Config.ButtonHeightRight*(button+1))-(Config.EntryYOffset*(button+3));
            }
            else
            {
                RectButtonsRight.at(button).x = Config.ScreenWidth-Config.ButtonWidthRight-Config.EntryXOffset;
                RectButtonsRight.at(button).y = Config.ScreenHeight-(Config.ButtonHeightRight*(button+1))-(Config.EntryYOffset*(button+3));
            }
            RectButtonsRight.at(button).w = Config.ButtonWidthRight;
            RectButtonsRight.at(button).h = Config.ButtonHeightRight;

            if (ButtonModesRight.at(button) != EVENT_NONE)
            {
                if (DrawButton( ButtonModesRight.at(button), Fonts.at(FONT_SIZE_LARGE), RectButtonsRight.at(button) ))
                {
                    return 1;
                }
            }
            UpdateRect( RectButtonsRight.at(button).x, RectButtonsRight.at(button).y, RectButtonsRight.at(button).w, RectButtonsRight.at(button).h );
        }
        DrawState_ButtonR = false;
    }

    //Display the preview graphic
    if ((Mode == MODE_SELECT_ENTRY) && (DrawState_Preview == true))
    {
        preview.x = Config.ScreenWidth-Config.PreviewWidth-Config.EntryXOffset;
        preview.y = Config.ScreenHeight-Config.PreviewHeight-(Config.ButtonHeightRight*3)-(Config.EntryYOffset*6);

        if (ImagePreview != NULL)
        {
            ApplyImage( preview.x, preview.y, ImagePreview, Screen, NULL );
        }
        UpdateRect( preview.x, preview.y, Config.PreviewWidth, Config.PreviewHeight );
        DrawState_Preview = false;
    }

    return 0;
}

int8_t CSelector::DrawButton( uint8_t button, TTF_Font* font, SDL_Rect& location )
{
    SDL_Surface* text_surface = NULL;
    SDL_Rect rect_text;

    if (ImageButtons.at(button) != NULL)
    {
        ApplyImage( location.x, location.y, ImageButtons.at(button), Screen, NULL );
    }
    else
    {
        SDL_FillRect( Screen, &location, rgb_to_int(Config.Colors.at(Config.ColorButton), PixelFormat) );
    }

    if (Config.ShowLabels == true)
    {
        text_surface = TTF_RenderText_Solid( font, LabelButtons.at(button).c_str(), Config.Colors.at(Config.ColorFontButton) );

        if (text_surface != NULL)
        {
            rect_text.x = location.x + ((location.w-text_surface->w)/2);
            rect_text.y = location.y + ((location.h-text_surface->h)/2);

            ApplyImage( rect_text.x, rect_text.y, text_surface, Screen, NULL );

            FREE_IMAGE( text_surface );
        }
        else
        {
            Log( __FILENAME__, __LINE__, "Failed to create TTF surface with TTF_RenderText_Solid: %s", TTF_GetError() );
            return 1;
        }
    }
    return 0;
}

int8_t CSelector::DrawText( SDL_Rect& location )
{
    int16_t         total;
    int16_t         prev_width;
    int16_t         prev_height;
    string          text;
    SDL_Rect        box, clip;

    prev_width  = 0;
    prev_height = 0;

    if (Config.AutoLayout == false)
    {
        location.x = Config.PosX_Title;
        location.y = Config.PosY_Title;
    }

    // Title text
    if (ImageTitle != NULL)
    {
        if (DrawState_Title == true)
        {
            ApplyImage( location.x, location.y, ImageTitle, Screen, NULL );
            UpdateRect( location.x, location.y, ImageTitle->w, ImageTitle->h );
            DrawState_Title = false;
        }
        location.y += ImageTitle->h + Config.PathYDelta;
    }

    // Entry Filter and Filepath (they can overlap so both are drawn when either change)
    if (Mode == MODE_SELECT_ENTRY)
    {
        if ((DrawState_FilePath == true) || (DrawState_Filter == true))
        {
            int16_t max_height;

            // Entry Filter
            if (ImageFilter != NULL)
            {
                prev_width  = ImageFilter->w;
                prev_height = ImageFilter->h;
            }
            else
            {
                prev_width  = 0;
                prev_height = 0;
            }
            max_height = prev_height;

            FREE_IMAGE( ImageFilter );

            if (Profile.EntryFilter.length() > 0)
            {
                ImageFilter = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_MEDIUM), Profile.EntryFilter.c_str(), Config.Colors.at(Config.ColorFontFiles) );
                if (ImageFilter != NULL)
                {
                    clip.x = 0;
                    clip.y = 0;
                    clip.w = Config.FilePathMaxWidth;
                    clip.h = ImageFilter->h;
                    if (ImageFilter->w > Config.FilePathMaxWidth)
                    {
                        clip.x = ImageFilter->w-Config.FilePathMaxWidth;
                    }

                    location.x = Config.ScreenWidth - ImageFilter->w - Config.EntryXOffset;

                    ApplyImage( location.x, location.y, ImageFilter, Screen, &clip );

                    max_height = MAX( max_height, ImageFilePath->h );
                }
                else
                {
                    Log( __FILENAME__, __LINE__, "Failed to create TTF surface with TTF_RenderText_Solid: %s", TTF_GetError() );
                    return 1;
                }
            }
            location.x = Config.EntryXOffset;

            // File path
            if (ImageFilePath != NULL)
            {
                prev_width  = ImageFilePath->w;
                prev_height = ImageFilePath->h;
            }
            else
            {
                prev_width  = 0;
                prev_height = 0;
            }
            max_height = MAX( max_height, prev_height );

            FREE_IMAGE(ImageFilePath);

            text = Profile.FilePath;
            if (Profile.ZipFile.length())
            {
                text += "->" + Profile.ZipFile;
            }

            ImageFilePath = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_MEDIUM), text.c_str(), Config.Colors.at(Config.ColorFontFiles) );
            if (ImageFilePath != NULL)
            {
                clip.x = 0;
                clip.y = 0;
                clip.w = Config.FilePathMaxWidth;
                clip.h = ImageFilePath->h;

                if (ImageFilePath->w > Config.FilePathMaxWidth)
                {
                    clip.x = ImageFilePath->w-Config.FilePathMaxWidth;
                }

                ApplyImage( location.x, location.y, ImageFilePath, Screen, &clip );

                max_height = MAX( max_height, ImageFilePath->h );
            }
            else
            {
                Log( __FILENAME__, __LINE__, "Failed to create TTF surface with TTF_RenderText_Solid: %s", TTF_GetError() );
                return 1;
            }

            UpdateRect( 0, location.y, Config.ScreenWidth, max_height );

            DrawState_FilePath = false;
            DrawState_Filter = false;
        }
    }

    if (ImageFilePath != NULL)
    {
        location.y += ImageFilePath->h + Config.EntryYOffset;
    }

    // About text
    if (ImageAbout != NULL)
    {
        if (DrawState_About == true)
        {
            box.x = Config.ScreenWidth  - ImageAbout->w - Config.EntryXOffset;
            box.y = Config.ScreenHeight - ImageAbout->h - Config.EntryYOffset;
            ApplyImage( box.x, box.y, ImageAbout, Screen, NULL );
            UpdateRect( box.x, box.y, ImageAbout->w, ImageAbout->h );
            DrawState_About = false;
        }
    }

    // Item count
    switch (Mode)
    {
        case MODE_SELECT_ENTRY:
            total = ItemsEntry.size();
            break;
        case MODE_SELECT_ARGUMENT: // fall through
        case MODE_SELECT_OPTION:
            total = ItemsArgument.size();
            break;
        case MODE_SELECT_VALUE:
            total = ItemsValue.size();
            break;
        default:
            total = 0;
            text = "Error: Unknown mode";
            break;
    }

    // Draw index
    if (DrawState_Index == true)
    {
        if (ImageIndex != NULL)
        {
            prev_width  = ImageIndex->w;
            prev_height = ImageIndex->h;
        }
        else
        {
            prev_width  = 0;
            prev_height = 0;
        }

        FREE_IMAGE( ImageIndex );

        text = "0 of 0";
        if (total > 0)
        {
            text = i_to_a(DisplayList.at(Mode).absolute+1) + " of " + i_to_a(total);
        }
        ImageIndex = TTF_RenderText_Solid(Fonts.at(FONT_SIZE_SMALL), text.c_str(), Config.Colors.at(Config.ColorFontFiles));

        if (ImageIndex != NULL)
        {
            box.x = Config.EntryXOffset;
            box.y = Config.ScreenHeight - ImageIndex->h - Config.EntryYOffset;

            ApplyImage( box.x, box.y, ImageIndex, Screen, NULL );
            UpdateRect( box.x, box.y, MAX(ImageIndex->w, prev_width), MAX(ImageIndex->h, prev_height) );
        }
        else
        {
            Log( __FILENAME__, __LINE__, "Failed to create TTF surface with TTF_RenderText_Solid: %s", TTF_GetError() );
            return 1;
        }
        DrawState_Index = false;
    }

    // Zip extract option
    if (DrawState_ZipMode == true)
    {
        if ((Config.UseZipSupport == true) && (Profile.ZipFile.length() > 0))
        {
            if (ImageZipMode != NULL)
            {
                prev_width  = ImageZipMode->w;
                prev_height = ImageZipMode->h;
            }
            else
            {
                prev_width  = 0;
                prev_height = 0;
            }

            FREE_IMAGE( ImageZipMode );

            if (ExtractAllFiles == true)
            {
                text = "Extract All";
            }
            else
            {
                text = "Extract Selection";
            }

            ImageZipMode = TTF_RenderText_Solid(Fonts.at(FONT_SIZE_SMALL), text.c_str(), Config.Colors.at(Config.ColorFontFiles));
            if (ImageZipMode == NULL)
            {
                Log( __FILENAME__, __LINE__, "Failed to create TTF surface with TTF_RenderText_Solid: %s", TTF_GetError() );
                return 1;
            }
        }

        if (ImageZipMode != NULL)
        {
            box.x = 5*Config.EntryXOffset;
            box.y = Config.ScreenHeight - ImageZipMode->h - Config.EntryYOffset;

            if ((Config.UseZipSupport == true) && (Profile.ZipFile.length() > 0))
            {
                ApplyImage( box.x, box.y, ImageZipMode, Screen, NULL );
            }
            UpdateRect( box.x, box.y, MAX(ImageZipMode->w, prev_width), MAX(ImageZipMode->h, prev_height) );
        }
        DrawState_ZipMode = false;
    }

#if defined(DEBUG)
    if (ImageDebug != NULL)
    {
        prev_width  = ImageDebug->w;
        prev_height = ImageDebug->h;
    }
    else
    {
        prev_width  = 0;
        prev_height = 0;
    }

    FREE_IMAGE( ImageDebug );

    text = "DEBUG abs " + i_to_a(DisplayList.at(Mode).absolute)  + " rel " + i_to_a(DisplayList.at(Mode).relative)
         + " F " + i_to_a(DisplayList.at(Mode).first) + " L " + i_to_a(DisplayList.at(Mode).last)
         + " T " + i_to_a(DisplayList.at(Mode).total)
         + " fps " + i_to_a(FPSDrawn) + " skp " + i_to_a(FPSSkip) + " slp " + i_to_a(FPSSleep)
         + " lp " + i_to_a(LoopTimeAverage);

    ImageDebug = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_SMALL), text.c_str(), Config.Colors.at(Config.ColorFontFiles) );

    if (ImageDebug != NULL)
    {
        box.x = Config.EntryXOffset;
        box.y = Config.ScreenHeight - ImageDebug->h;

        ApplyImage( box.x, box.y, ImageDebug, Screen, NULL );
        UpdateRect( box.x, box.y, MAX(ImageDebug->w, prev_width), MAX(ImageDebug->h, prev_height) );
    }
    else
    {
        Log( __FILENAME__, __LINE__, "Failed to create TTF surface with TTF_RenderText_Solid: %s", TTF_GetError() );
        return 1;
    }
#endif

    return 0;
}

int8_t CSelector::RunExec( uint16_t selection )
{
    bool entry_found;
    int16_t ext_index;
    string filename;
    string filepath;
    string command;
    string extension;
    string value;
    string cmdpath, cmdname;
    vector<string> commands;
    entry_t* entry = NULL;
    argforce_t* argforce = NULL;
    exeforce_t* exeforce = NULL;
    argument_t* argument = NULL;

    // Find a entry for argument values
    entry_found = false;

    if (!CheckRange( selection, ItemsEntry.size() ))
    {
        Log( __FILENAME__, __LINE__, "Error: RunExec selection is out of range" );
        return 1;
    }

    if (ItemsEntry.at(selection).Entry >= 0)
    {
        entry = &Profile.Entries.at(ItemsEntry.at(selection).Entry);
        if (entry->Custom == true)
        {
            entry_found = true;
        }
    }

    // Find a executable for file extension
    filename = ItemsEntry.at(selection).Name;
    if (ItemsEntry.at(selection).Type == TYPE_DIR)
    {
        extension = EXT_DIRS;
    }
    else
    {
        extension = filename.substr( filename.find_last_of(".")+1 );
    }

    ext_index = Profile.FindExtension( extension );

    if (CheckRange( ext_index, Profile.Extensions.size() ))
    {
        command.clear();

        // Unzip if needed
        if ((Config.UseZipSupport == true) && (Profile.ZipFile.length() > 0))
        {
            mkdir( Config.ZipPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
            if (ExtractAllFiles == true)    // Extract all
            {
                Profile.Minizip.ExtractFiles( Profile.FilePath + Profile.ZipFile, Config.ZipPath );
            }
            else                            // Extract one
            {
                Profile.Minizip.ExtractFile( Profile.FilePath + Profile.ZipFile, Config.ZipPath, filename );
            }

            filepath = Config.ZipPath + "/";
        }
        else
        {
            filepath = Profile.FilePath;
        }


        // Setup commands
        for (uint16_t i=0; i<Profile.Commands.size(); i++)
        {
            //command += "cd " + Profile.Commands.at(i).Path + "; ";
            command += Profile.Commands.at(i).Path + Profile.Commands.at(i).Command;

            for (uint16_t j=0; j<Profile.Commands.at(i).Arguments.size(); j++)
            {
                if (Profile.Commands.at(i).Arguments.at(j).Flag.compare(VALUE_NOVALUE) != 0)
                {
                    command += Profile.Commands.at(i).Arguments.at(j).Flag;
                }

                if (Profile.Commands.at(i).Arguments.at(j).Flag.compare(VALUE_FLAGONLY) !=0 )
                {
                    if (   (entry_found == true)
                        && (entry->CmdValues.at(i) > DEFAULT_VALUE)
                       )
                    {
                        command += Profile.Commands.at(i).Arguments.at(j).Values.at(entry->CmdValues.at(i));
                    }
                    else
                    {
                        command += Profile.Commands.at(i).Arguments.at(j).Values.at(Profile.Commands.at(i).Arguments.at(j).Default);
                    }
                }
            }
            command += "; ";
        }

        // Check exe forces
        cmdpath = Profile.Extensions.at(ext_index).exePath;
        cmdname = Profile.Extensions.at(ext_index).exeName;

        for (uint16_t j=0; j<Profile.Extensions.at(ext_index).ExeForces.size(); j++)
        {
            exeforce = &Profile.Extensions.at(ext_index).ExeForces.at(j);
            for (uint16_t k=0; k<exeforce->Files.size(); k++)
            {
                if (exeforce->Files.at(k).compare( filename ) == 0)
                {
                    cmdpath = exeforce->exePath;
                    cmdname = exeforce->exeName;
                    break;
                }
            }
        }

        // Add Executable to command
        command += "cd " + cmdpath + "; ";
        command += "LD_LIBRARY_PATH=./; export LD_LIBRARY_PATH; ";
        command += "./" + cmdname;

        // Setup arguments
        for (uint16_t i=0; i<Profile.Extensions.at(ext_index).Arguments.size(); i++)
        {
            value.clear();
            argument = &Profile.Extensions.at(ext_index).Arguments.at(i);

            // Check arg forces
            for (uint16_t j=0; j<Profile.Extensions.at(ext_index).ArgForces.size(); j++)
            {
                argforce = &Profile.Extensions.at(ext_index).ArgForces.at(j);
                if (argforce->Path.compare( Profile.FilePath ) == 0)
                {
                    if (i == argforce->Argument)
                    {
                        Log( __FILENAME__, __LINE__, "Setting argforce on arg %d", i );
                        value = argforce->Value;
                        break;
                    }
                }
            }
            // Check arguments for default value or custom entry value
            if (value.length() == 0 )
            {
                if (   (entry_found == true)
                    && (CheckRange( i, entry->ArgValues.size() ))
                    && (entry->ArgValues.at(i) > DEFAULT_VALUE)
                   )
                {
                    value = argument->Values.at( entry->ArgValues.at(i) );
                }
                else
                {
                    if (CheckRange( argument->Default, argument->Values.size() ))
                    {
                        value = argument->Values.at( argument->Default );
                    }
                    else
                    {
                        Log( __FILENAME__, __LINE__, "Error: RunExec argument->Default is out of range" );
                        return 1;
                    }
                }
            }
            // Add the argument if used
            if (value.length() > 0)
            {
                if (value.compare( VALUE_NOVALUE ) != 0 )
                {
                    command += " " + argument->Flag + " ";

                    if (value.compare( VALUE_FLAGONLY ) !=0 )
                    {
                        if (value.compare( VALUE_FILENAME ) == 0)
                        {
                            if (Config.FilenameArgNoExt == true)
                            {
                                string::size_type pos = filename.find_last_of(".");
                                if (pos != string::npos)
                                    filename.resize( pos );
                            }

                            if (entry_found==true)
                            {
                                command += '\"';
                                if (Config.FilenameAbsPath == true)
                                {
                                    command += entry->Path;
                                }
                                command += entry->Name + '\"';
                            }
                            else
                            {
                                command += '\"';
                                if (Config.FilenameAbsPath == true)
                                {
                                    command += filepath;
                                }
                                command += filename + '\"';
                            }
                        }
                        else
                        {
                            command += value;
                        }
                    }
                }
            }
        }
    }
    else
    {
        Log( __FILENAME__, __LINE__, "Warning no extension was found for this file type" );
        return 1;
    }

    command += "; sync;";
    if (Config.ReloadLauncher == true)
    {
        command += " cd " + Profile.LauncherPath + ";";
        command += " exec ./" + Profile.LauncherName ;
        // Arguments
        command += " " + string(ARG_PROFILE) + " " + ProfilePath;
        command += " " + string(ARG_CONFIG)  + " " + ConfigPath;
        command += " " + string(ARG_ZIPLIST) + " " + ZipListPath;
    }

    /* Print out all the commands in a list form  */
    SplitString( ";", command, commands );

    Log( __FILENAME__, __LINE__, "Running command start:" );
    for(uint16_t i=0; i<commands.size(); i++)
    {
        Log( __FILENAME__, __LINE__, "%02d '%s'", i, commands.at(i).c_str() );
    }
    Log( __FILENAME__, __LINE__, "Running command end:" );

    CloseResources(0);

    execlp( "/bin/sh", "/bin/sh", "-c", command.c_str(), NULL );

    //if execution continues then something went wrong and as we already called SDL_Quit we cannot continue, try reloading
    Log( __FILENAME__, __LINE__, "Error executing selected application, re-launching %s", APPNAME );

    chdir( Profile.LauncherPath.c_str() );
    execlp( Profile.LauncherName.c_str(), Profile.LauncherName.c_str(), NULL );

    return 0;
}

int8_t CSelector::PollInputs( void )
{
    int16_t     newsel;
    string      keyname;
    SDL_Event   event;

    for (uint16_t index=0; index<EventReleased.size(); index++)
    {
        if (EventReleased.at(index) == true)
        {
            EventReleased.at(index)     = false;
            EventPressCount.at(index)   = EVENT_LOOPS_OFF;
#if defined(DEBUG)
            //Log( __FILENAME__, __LINE__, "DEBUG EventReleased %d", index );
#endif
        }
        else if (EventPressCount.at(index) != EVENT_LOOPS_OFF)
        {
            EventPressCount.at(index)--;
            if (EventPressCount.at(index) < EVENT_LOOPS_ON)
            {
                EventPressCount.at(index) = EVENT_LOOPS;
            }
        }
    }

    // Sanity check
    if (!CheckRange( Mode, DisplayList.size() ))
    {
        Log( __FILENAME__, __LINE__, "Error: PollInputs Mode out of range" );
        return 1;
    }

    while (SDL_PollEvent( &event ))
    {
        switch( event.type )
        {
            case SDL_KEYDOWN:
                keyname = SDL_GetKeyName( event.key.keysym.sym );
                if ((keyname.length() == 1) || (event.key.keysym.sym == SDLK_BACKSPACE))
                {
                    if (Config.EntryFastMode == ENTRY_FAST_MODE_ALPHA)
                    {
                        DisplayList.at(Mode).absolute = Profile.AlphabeticIndices.at(event.key.keysym.sym-SDLK_a);
                        Profile.EntryFilter.clear();
                    }
                    else if (Config.EntryFastMode == ENTRY_FAST_MODE_FILTER)
                    {
                        if ((event.key.keysym.sym==SDLK_BACKSPACE) || (event.key.keysym.sym==SDLK_DELETE))
                        {
                            if (Profile.EntryFilter.length() > 0)
                            {
                                Profile.EntryFilter.erase( Profile.EntryFilter.length()-1 );
                            }
                        }
                        else
                        {
                            Profile.EntryFilter += keyname;
                        }
                        DrawState_Filter    = true;
                        Rescan              = true;
                    }
                }
                else
                {
                    for (uint16_t index=0; index<EventPressCount.size(); index++)
                    {
                        if (event.key.keysym.sym == Config.KeyMaps.at(index))
                        {
                            EventPressCount.at(index) = EVENT_LOOPS_ON;
                        }
                        else if (Config.UnusedKeysLaunch)
                        {
                            EventPressCount.at(EVENT_SELECT) = EVENT_LOOPS_ON;
                        }
                    }
                }
#if defined(DEBUG)
                Log( __FILENAME__, __LINE__, "DEBUG SDL_KEYDOWN name: %s sym: %d", keyname.c_str(), event.key.keysym.sym );
#endif
                break;

            case SDL_KEYUP:
                for (uint16_t index=0; index<EventReleased.size(); index++)
                {
                    if (event.key.keysym.sym == Config.KeyMaps.at(index))
                    {
                        EventReleased.at(index) = true;
                    }
                    else if (Config.UnusedKeysLaunch)
                    {
                        EventReleased.at(EVENT_SELECT) = true;
                    }
                }
#if defined(DEBUG)
                keyname = SDL_GetKeyName( event.key.keysym.sym );
                Log( __FILENAME__, __LINE__, "DEBUG SDL_KEYUP name: %s sym: %d", keyname.c_str(), event.key.keysym.sym );
#endif
                break;

            case SDL_JOYBUTTONDOWN:
                for (uint16_t index=0; index<EventPressCount.size(); index++)
                {
                    if (event.jbutton.button == Config.JoyMaps.at(index))
                    {
                        EventPressCount.at(index) = EVENT_LOOPS_ON;
                    }
                    else if (Config.UnusedJoysLaunch)
                    {
                        EventPressCount.at(EVENT_SELECT) = EVENT_LOOPS_ON;
                    }
                }
#if defined(DEBUG)
                Log( __FILENAME__, __LINE__, "DEBUG SDL_JOYBUTTONDOWN button: %d", event.jbutton.button );
#endif
                break;

            case SDL_JOYBUTTONUP:
                for (uint16_t index=0; index<EventReleased.size(); index++)
                {
                    if (event.jbutton.button == Config.JoyMaps.at(index))
                    {
                        EventReleased.at(index) = true;
                    }
                    else if (Config.UnusedJoysLaunch)
                    {
                        EventReleased.at(EVENT_SELECT) = true;
                    }
                }
#if defined(DEBUG)
                Log( __FILENAME__, __LINE__, "DEBUG SDL_JOYBUTTONUP button: %d", event.jbutton.button );
#endif
                break;

            case SDL_JOYAXISMOTION:
                if (event.jaxis.value < -Config.AnalogDeadZone)
                {
                    if (event.jaxis.axis == 0)
                    {
                        if (IsEventOff(EVENT_PAGE_UP))
                        {
                            EventPressCount.at(EVENT_PAGE_UP) = EVENT_LOOPS_ON;
                        }
                        EventReleased.at(EVENT_PAGE_DOWN) = true;
                    }

                    if (event.jaxis.axis == 1)
                    {
                        if (IsEventOff(EVENT_ONE_UP))
                        {
                            EventPressCount.at(EVENT_ONE_UP) = EVENT_LOOPS_ON;
                        }
                        EventReleased.at(EVENT_ONE_DOWN) = true;
                    }
                }
                else if (event.jaxis.value > Config.AnalogDeadZone)
                {
                    if (event.jaxis.axis == 0)
                    {
                        if (IsEventOff(EVENT_PAGE_DOWN))
                        {
                            EventPressCount.at(EVENT_PAGE_DOWN) = EVENT_LOOPS_ON;
                        }
                        EventReleased.at(EVENT_PAGE_UP) = true;
                    }

                    if (event.jaxis.axis == 1)
                    {
                        if (IsEventOff(EVENT_ONE_DOWN))
                        {
                            EventPressCount.at(EVENT_ONE_DOWN) = EVENT_LOOPS_ON;
                        }
                        EventReleased.at(EVENT_ONE_UP) = true;
                    }
                }
                else
                {
                    EventReleased.at(EVENT_ONE_UP)      = true;
                    EventReleased.at(EVENT_ONE_DOWN)    = true;
                    EventReleased.at(EVENT_PAGE_UP)     = true;
                    EventReleased.at(EVENT_PAGE_DOWN)   = true;
                }
#if defined(DEBUG)
                Log( __FILENAME__, __LINE__, "DEBUG SDL_JOYAXISMOTION axis: %d value: %d", event.jaxis.axis, event.jaxis.value );
#endif
                break;

            case SDL_JOYHATMOTION:
                if (event.jhat.value == SDL_HAT_UP)
                {
                    EventPressCount.at(EVENT_ONE_UP) = EVENT_LOOPS_ON;
                }
                if (event.jhat.value == SDL_HAT_DOWN)
                {
                    EventPressCount.at(EVENT_ONE_DOWN) = EVENT_LOOPS_ON;
                }
                if (event.jhat.value == SDL_HAT_LEFT)
                {
                    EventPressCount.at(EVENT_PAGE_UP) = EVENT_LOOPS_ON;
                }
                if (event.jhat.value == SDL_HAT_RIGHT)
                {
                    EventPressCount.at(EVENT_PAGE_DOWN) = EVENT_LOOPS_ON;
                }
                if (event.jhat.value == SDL_HAT_CENTERED)
                {
                    EventReleased.at(EVENT_ONE_UP)      = true;
                    EventReleased.at(EVENT_ONE_DOWN)    = true;
                    EventReleased.at(EVENT_PAGE_UP)     = true;
                    EventReleased.at(EVENT_PAGE_DOWN)   = true;
                }
                break;

            case SDL_MOUSEMOTION:
                Mouse.x  = event.motion.x;
                Mouse.y  = event.motion.y;

                for (uint16_t index=0; index<RectEntries.size(); index++)
                {
                    if (CheckRectCollision( &Mouse, &RectEntries.at(index) ))
                    {
                        // Determine item being hovered over by the pointer
                        newsel = DisplayList.at(Mode).first+index;
                        // Only need to refresh the names when a new entry is hovered
                        if (newsel != DisplayList.at(Mode).absolute)
                        {
                            DisplayList.at(Mode).absolute = newsel;
                            DisplayList.at(Mode).relative = index;
                            RefreshList = true;
                        }
                        break;
                    }
                }
#if defined(DEBUG)
                Log( __FILENAME__, __LINE__, "DEBUG SDL_MOUSEMOTION x: %d y: %d", event.motion.x, event.motion.y );
#endif
                break;

            case SDL_MOUSEBUTTONDOWN:
                switch (event.button.button)
                {
                    case SDL_BUTTON_LEFT:
                    case SDL_BUTTON_MIDDLE:
                    case SDL_BUTTON_RIGHT:;
                        Mouse.x  = event.button.x;
                        Mouse.y  = event.button.y;
                        for (uint16_t index=0; index<RectButtonsLeft.size(); index++)
                        {
                            if (ButtonModesLeft.at(index)!=EVENT_NONE)
                            {
                                if (CheckRectCollision( &Mouse, &RectButtonsLeft.at(index) ))
                                {
                                    EventPressCount.at(ButtonModesLeft.at(index)) = EVENT_LOOPS_ON;
#if defined(DEBUG)
                                    Log( __FILENAME__, __LINE__, "DEBUG LeftButton Active %d %d", ButtonModesLeft.at(index), index );
#endif
                                }
                            }
                        }
                        for (uint16_t index=0; index<RectButtonsRight.size(); index++)
                        {
                            if (ButtonModesRight.at(index)!=EVENT_NONE)
                            {
                                if (CheckRectCollision( &Mouse, &RectButtonsRight.at(index) ))
                                {
                                    EventPressCount.at(ButtonModesRight.at(index)) = EVENT_LOOPS_ON;
#if defined(DEBUG)
                                    Log( __FILENAME__, __LINE__, "DEBUG RightButton Active %d %d", ButtonModesRight.at(index), index );
#endif
                                }
                            }
                        }
                        break;
#if !SDL_VERSION_ATLEAST(2,0,0)
                    case SDL_BUTTON_WHEELUP:
                        EventPressCount.at(EVENT_ONE_UP) = EVENT_LOOPS_ON;
                        break;
                    case SDL_BUTTON_WHEELDOWN:
                        EventPressCount.at(EVENT_ONE_DOWN) = EVENT_LOOPS_ON;
                        break;
#endif
                    default:
                        break;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                switch (event.button.button)
                {
                    case SDL_BUTTON_LEFT:
                    case SDL_BUTTON_MIDDLE:
                    case SDL_BUTTON_RIGHT:
#if !SDL_VERSION_ATLEAST(2,0,0)
                    case SDL_BUTTON_WHEELUP:
                    case SDL_BUTTON_WHEELDOWN:
#endif
                        Mouse.x  = event.button.x;
                        Mouse.y  = event.button.y;

                        for (uint16_t index=0; index<EventReleased.size(); index++)
                        {
                            EventReleased.at(index) = true;
                        }
#if defined(DEBUG)
                        Log( __FILENAME__, __LINE__, "DEBUG Releasing all mouse events" );
#endif
                        break;
                    default:
                        break;
                }
                break;
#if SDL_VERSION_ATLEAST(2,0,0)
            case SDL_MOUSEWHEEL:
#if defined(DEBUG)
                Log( __FILENAME__, __LINE__, "DEBUG mouse wheel %d", event.wheel.y );
#endif
                if(event.wheel.y > 0)
                {
                    EventPressCount.at(EVENT_ONE_UP) = EVENT_LOOPS_ON;
                    EventReleased.at(EVENT_ONE_UP)   = true;
                }
                if(event.wheel.y < 0)
                {
                    EventPressCount.at(EVENT_ONE_DOWN) = EVENT_LOOPS_ON;
                    EventReleased.at(EVENT_ONE_DOWN)   = true;
                }
                break;
#endif
            default:
                break;
        }
    }

    // List navigation
    if (IsEventOn(EVENT_ONE_UP)==true)
    {
        DisplayList.at(Mode).absolute--;
        DisplayList.at(Mode).relative--;
        RefreshList = true;
    }
    if (IsEventOn(EVENT_ONE_DOWN)==true)
    {
        DisplayList.at(Mode).absolute++;
        DisplayList.at(Mode).relative++;
        RefreshList = true;
    }
    if (IsEventOn(EVENT_PAGE_UP)==true)
    {
        DisplayList.at(Mode).absolute -= Config.MaxEntries;
        DisplayList.at(Mode).relative = 0;
        RefreshList = true;
    }
    if (IsEventOn(EVENT_PAGE_DOWN)==true)
    {
        DisplayList.at(Mode).absolute += Config.MaxEntries;
        DisplayList.at(Mode).relative = Config.MaxEntries-1;
        RefreshList = true;
    }

    // Path navigation
    if (Mode == MODE_SELECT_ENTRY)
    {
        // Go up into a dir
        if ((Rescan == false) && (IsEventOn(EVENT_DIR_UP) == true))
        {
            if ((Config.UseZipSupport == 1) && (Profile.ZipFile.length() > 0))
            {
                ZipUp();
            }
            else
            {
                if (Profile.LaunchableDirs == false)
                {
                    DirectoryUp();
                }
            }
        }

        // Go down into a dir
        if (   (Rescan == false)
            && (   (IsEventOn(EVENT_DIR_DOWN) == true)
                || (IsEventOn(EVENT_SELECT) == true)
               )
           )
        {
            if (ItemsEntry.size()>0)
            {
                if (ItemsEntry.at(DisplayList.at(Mode).absolute).Type == TYPE_DIR)
                {
                    if (Profile.LaunchableDirs == false)
                    {
                        DirectoryDown();
                    }
                }
                else if ((Config.UseZipSupport == 1) && (ItemsEntry.at(DisplayList.at(Mode).absolute).Type == TYPE_ZIP))
                {
                    ZipDown();
                }
            }
            else
            {
                EventPressCount.at( EVENT_SELECT ) = EVENT_LOOPS_OFF;
            }
        }
    }

    // Value configuration
    if (Mode == MODE_SELECT_VALUE)
    {
        SetOneEntryValue = false;
        SetAllEntryValue = false;
        if (IsEventOn(EVENT_SET_ONE) == true)
        {
            RefreshList         = true;
            SetOneEntryValue    = true;
        }
        if (IsEventOn(EVENT_SET_ALL) == true)
        {
            RefreshList         = true;
            SetAllEntryValue    = true;
        }
    }

    if (IsEventOn(EVENT_ZIP_MODE) == true)
    {
        Redraw              = true;
        DrawState_ZipMode   = true;
        ExtractAllFiles = !ExtractAllFiles;
    }

    return 0;
}
