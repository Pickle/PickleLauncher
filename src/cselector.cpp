/**
 *  @section LICENSE
 *
 *  PickleLauncher
 *  Copyright (C) 2010-2011 Scott Smith
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
        Mode                (MODE_SELECT_ENTRY),
        LastSelectedEntry   (0),
        TextScrollOffset    (0),
        CurScrollSpeed      (0),
        CurScrollPause      (0),
#if defined(DEBUG)
        FramesDrawn         (0),
        FramesSkipped       (0),
        FPSDrawn            (0),
        FPSSkip             (0),
        FrameCountTime      (0),
#endif
        FrameEndTime        (0),
        FrameStartTime      (0),
        FrameDelay          (0),
        Mouse               (),
        Joystick            (NULL),
        Screen              (NULL),
        ImageBackground     (NULL),
        ImagePointer        (NULL),
        ImageSelectPointer  (NULL),
        ImagePreview        (NULL),
        ImageButtons        (),
        Fonts               (),
        Config              (),
        Profile             (),
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
        RectButtonsRight    ()
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
    int16_t selection;

    result = 0;

    ProcessArguments( argc, argv );

    // Load video,input,profile resources
    if (OpenResources())
    {
        result = 1;
    }

    // Display and poll the user for a selection
    if (result == 0)
    {
        selection = DisplayScreen();

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
    uint8_t arg_index;
    string launcher;
    string argument;

    launcher = string(argv[0]);
    Profile.LauncherName = launcher.substr( launcher.find_last_of('/')+1 );
    Profile.LauncherPath = launcher.substr( 0, launcher.find_last_of('/')+1 );

    Log( "Running from '%s' as '%s'\n", Profile.LauncherPath.c_str(), Profile.LauncherName.c_str() );

    for (arg_index=0; arg_index<argc; arg_index++ )
    {
        argument = string(argv[arg_index]);

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
    uint8_t button_index;
    uint32_t flags;

    Log( "Loading config.\n" );
    if (Config.Load( ConfigPath ))
    {
        Log( "Failed to load config\n" );
        return 1;
    }

    Log( "Loading ziplist.\n" );
    if (Config.UseZipSupport == true && Profile.Minizip.LoadUnzipList( ZipListPath ))
    {
        Log( "Failed to load ziplist\n" );
        return 1;
    }

    // Initialize defaults, Video and Audio subsystems
    Log( "Initializing SDL.\n" );
    if (SDL_Init( SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK )==-1)
    {
        Log( "Failed to initialize SDL: %s.\n", SDL_GetError() );
        return 1;
    }
    Log( "SDL initialized.\n" );

    // Setup SDL Screen
    flags = SCREEN_FLAGS;
    if (Config.Fullscreen == true)
    {
        flags |= SDL_FULLSCREEN;
    }
    Screen = SDL_SetVideoMode( Config.ScreenWidth, Config.ScreenHeight, Config.ScreenDepth, flags );
    if (Screen == NULL)
    {
        Log( "Failed to %dx%dx%d video mode: %s\n", Config.ScreenWidth, Config.ScreenHeight, Config.ScreenDepth, SDL_GetError() );
        return 1;
    }

    // Load joystick
#if !defined(PANDORA) && !defined(X86)
    Joystick = SDL_JoystickOpen(0);
    if (Joystick == NULL)
    {
        Log( "Warning failed to open first joystick: %s\n", SDL_GetError() );
    }
#endif

    // Setup TTF SDL
    if (TTF_Init() == -1)
    {
        Log( "Failed to init TTF_Init: %s\n", TTF_GetError() );
        return 1;
    }

    // Load ttf font
    Fonts.at(FONT_SIZE_SMALL) = TTF_OpenFont( Config.PathFont.c_str(), Config.FontSizes.at(FONT_SIZE_SMALL) );
    if (!Fonts.at(FONT_SIZE_SMALL))
    {
        Log( "Failed to open small TTF_OpenFont: %s\n", TTF_GetError() );
        return 1;
    }
    Fonts.at(FONT_SIZE_MEDIUM) = TTF_OpenFont( Config.PathFont.c_str(), Config.FontSizes.at(FONT_SIZE_MEDIUM) );
    if (!Fonts.at(FONT_SIZE_MEDIUM))
    {
        Log( "Failed to open medium TTF_OpenFont: %s\n", TTF_GetError() );
        return 1;
    }
    Fonts.at(FONT_SIZE_LARGE) = TTF_OpenFont( Config.PathFont.c_str(), Config.FontSizes.at(FONT_SIZE_LARGE) );
    if (!Fonts.at(FONT_SIZE_LARGE))
    {
        Log( "Failed to open large TTF_OpenFont: %s\n", TTF_GetError() );
        return 1;
    }

    // Load images
    ImageBackground = LoadImage( Config.PathBackground );
    for (button_index=0; button_index<Config.PathButtons.size(); button_index++)
    {
        ImageButtons.at(button_index) = LoadImage( Config.PathButtons.at(button_index) );
    }

    //      Mouse pointer
    if (Config.ShowPointer==true)
    {
        ImagePointer = LoadImage( Config.PathPointer );
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
    ImageSelectPointer = LoadImage( Config.PathSelectPointer );
    if (ImageSelectPointer == NULL)
    {
        ImageSelectPointer = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_MEDIUM), ENTRY_ARROW, Config.Colors.at(COLOR_BLACK) );
    }

    Log( "Loading profile.\n" );
    if (Profile.Load( ProfilePath, Config.Delimiter ))
    {
        Log( "Failed to load profile\n" );
        return 1;
    }

    return 0;
}

void CSelector::CloseResources( int8_t result )
{
    uint8_t button_index;

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
        Log( "Closing SDL Joystick.\n" );
        SDL_JoystickClose( Joystick );
        Joystick = NULL;
    }

    // Close fonts
    Log( "Closing TTF fonts.\n" );
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
    if (ImageBackground != NULL)
    {
        SDL_FreeSurface( ImageBackground );
        ImageBackground = NULL;
    }
    for (button_index=0; button_index<ImageButtons.size(); button_index++)
    {
        if (ImageButtons.at(button_index) != NULL)
        {
            SDL_FreeSurface( ImageButtons.at(button_index) );
            ImageButtons.at(button_index) = NULL;
        }
    }

    if (ImagePointer != NULL)
    {
        SDL_FreeSurface( ImagePointer );
        ImagePointer = NULL;
    }

    if (ImageSelectPointer != NULL)
    {
        SDL_FreeSurface( ImageSelectPointer );
        ImageSelectPointer = NULL;
    }

    if (ImagePreview != NULL)
    {
        SDL_FreeSurface( ImagePreview );
        ImagePreview = NULL;
    }

    Log( "Quitting TTF.\n" );
    TTF_Quit();

    Log( "Quitting SDL.\n" );
    SDL_Quit();
}

int16_t CSelector::DisplayScreen( void )
{
    while (IsEventOff(EVENT_QUIT) == true && (IsEventOff(EVENT_SELECT) == true || Mode != MODE_SELECT_ENTRY) )
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
        FlipScreen();
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

void CSelector::FlipScreen( void )
{
    if (SkipFrame == false && Redraw == true)
    {
        if (SDL_Flip( Screen ) != 0)
        {
            Log( "Failed to swap the buffers: %s\n", SDL_GetError() );
        }

        Redraw = false;
#if defined(DEBUG)
        FramesDrawn++;
    }
    else
    {
        FramesSkipped++;
#endif
    }

    FrameEndTime = SDL_GetTicks();
    FrameDelay   = (MS_PER_SEC/FRAMES_PER_SEC) - (FrameEndTime - FrameStartTime);

    if (FrameDelay < 0)
    {
        SkipFrame = true;
    }
    else
    {
        SkipFrame = false;
        if (FrameDelay < MS_PER_SEC)
        {
            SDL_Delay( FrameDelay );
        }
    }
    FrameStartTime = SDL_GetTicks();

#if defined(DEBUG)
    if (FrameStartTime - FrameCountTime >= MS_PER_SEC)
    {
        FrameCountTime  = FrameStartTime;
        FPSDrawn        = FramesDrawn;
        FPSSkip         = FramesSkipped;
        FramesDrawn     = 0;
        FramesSkipped   = 0;
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
                    if (ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Type == TYPE_FILE ||
                       (ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Type == TYPE_DIR && Profile.LaunchableDirs == true))
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
            Log( "Error: Unknown Mode\n" );
            break;
    }

    if (Mode != old_mode)
    {
        Rescan      = true;
        RefreshList = true;
    }
}

int8_t CSelector::DisplaySelector( void )
{
    SDL_Rect rect_pos = { Config.EntryXOffset, Config.EntryYOffset, 0 ,0 };

    if (Rescan)
    {
        RescanItems();
        Rescan = false;
    }

    if (RefreshList)
    {
        PopulateList();
        RefreshList = false;
        Redraw      = true;
    }

    if (Redraw == true || CurScrollPause != 0 || CurScrollSpeed != 0 || TextScrollOffset != 0)
    {
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
        if (Config.ShowPointer == true && ImagePointer != NULL)
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
        Rescan      = true;
        RefreshList = true;
    }
    else
    {
        Log( "Error: Filepath is empty\n" );
    }
}

void CSelector::DirectoryDown( void )
{
    if (ItemsEntry.size()>0)
    {
        if (DisplayList.at(MODE_SELECT_ENTRY).absolute < (int16_t)ItemsEntry.size() )
        {
            if (ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Type == TYPE_DIR )
            {
                Profile.FilePath += ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Name + '/';
                Rescan      = true;
                RefreshList = true;

                EventPressCount.at(EVENT_SELECT) = EVENT_LOOPS_OFF;
            }
        }
        else
        {
            Log( "Error: Item index of %d too large for size of scanitems %d\n", DisplayList.at(MODE_SELECT_ENTRY).absolute, ItemsEntry.size() );
        }
    }
    else
    {
        EventPressCount.at( EVENT_SELECT ) = EVENT_LOOPS_OFF;
    }
}

void CSelector::ZipUp( void )
{
    Rescan = true;
    RefreshList = true;
    Profile.ZipFile = "";
}

void CSelector::ZipDown( void )
{
    Rescan = true;
    RefreshList = true;
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
            Log( "Error: Unknown Mode\n" );
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

    DisplayList.at( Mode ).absolute  = 0;
    DisplayList.at( Mode ).relative  = 0;
    DisplayList.at( Mode ).first     = 0;
    DisplayList.at( Mode ).last      = 0;
    DisplayList.at( Mode ).total     = total;
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
            Log( "Error: CSelector::PopulateList Unknown Mode\n" );
            break;
    }
}

void CSelector::PopModeEntry( void )
{
    uint16_t i;
    uint16_t index;

    for (i=0; i<ListNames.size(); i++)
    {
        index = DisplayList.at( MODE_SELECT_ENTRY ).first+i;
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

                // Load preview
                LoadPreview( ListNames.at(i).text );
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
            Log( "Error: CSelector::PopulateModeSelectEntry Index Error\n" );
        }
    }
}

void CSelector::PopModeArgument( void )
{
    uint16_t i;
    uint16_t index;

    for (i=0; i<ListNames.size(); i++)
    {
        index = DisplayList.at(MODE_SELECT_ARGUMENT).first+i;

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
            Log( "Error: PopModeArgument index is out of range\n" );
        }
    }
}

void CSelector::PopModeValue( void )
{
    uint16_t i;
    uint16_t index;
    int16_t defvalue;
    int16_t entry;
    listoption_t argument;

    if (CheckRange(DisplayList.at(MODE_SELECT_ARGUMENT).absolute, ItemsArgument.size() ))
    {
        argument = ItemsArgument.at(DisplayList.at(MODE_SELECT_ARGUMENT).absolute);

        for (i=0; i<ListNames.size(); i++)
        {
            index = DisplayList.at(MODE_SELECT_VALUE).first+i;

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
                if (index==defvalue)
                {
                    ListNames.at(i).text += '*';
                }

                // Set the color for the selected item for the entry
                ListNames.at(i).color = Config.ColorFontFiles;
                if (ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry<0)
                {
                    // A custom value has been selected, so create a new entry
                    if (SetOneEntryValue == true)
                    {
                        entry = Profile.AddEntry( argument, ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Name );
                        if (entry>0)
                        {
                            ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry = entry;
                        }
                        else
                        {
                            Log( "Error: Could not create new entry\n" );
                        }
                    }
                    else if (index==defvalue)
                    {
                        ListNames.at(i).color = COLOR_RED;
                    }
                }

                if (ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry>=0)
                {
                    if (ItemsArgument.at(DisplayList.at(MODE_SELECT_ARGUMENT).absolute).Command >= 0)
                    {
                        if (SetOneEntryValue == true || SetAllEntryValue == true)
                        {
                            Profile.Entries.at(ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry).CmdValues.at(argument.Command+argument.Argument) = DisplayList.at(MODE_SELECT_VALUE).absolute;
                        }
                        if (index == Profile.Entries.at(ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry).CmdValues.at(argument.Command+argument.Argument))
                        {
                            ListNames.at(i).color = COLOR_RED;
                        }
                    }
                    else
                    {
                        if (SetOneEntryValue == true || SetAllEntryValue == true)
                        {
                            Profile.Entries.at(ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry).ArgValues.at(argument.Argument) = DisplayList.at(MODE_SELECT_VALUE).absolute;
                        }
                        if (index == Profile.Entries.at(ItemsEntry.at(DisplayList.at(MODE_SELECT_ENTRY).absolute).Entry).ArgValues.at(argument.Argument))
                        {
                            ListNames.at(i).color = COLOR_RED;
                        }
                    }
                }
            }
            else
            {
                Log( "Error: PopModeValue index is out of range\n" );
            }
        }
    }
    else
    {
        Log( "Error: PopModeValue argument index out of range\n" );
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
        SDL_FreeSurface( ImagePreview );
        ImagePreview = NULL;
    }

    filename = Config.PreviewsPath + "/" + name.substr( 0, name.find_last_of(".")) + ".png";
    preview = LoadImage( filename.c_str() );
    if (preview != NULL)
    {
        ImagePreview = ScaleSurface( preview, Config.PreviewWidth, Config.PreviewHeight );
        SDL_FreeSurface( preview );
    }
}

int8_t CSelector::DrawNames( SDL_Rect& location )
{
    uint16_t entry_index;
    int16_t offset;
    SDL_Rect rect_clip;
    SDL_Surface* text_surface = NULL;

    if (Config.AutoLayout == false)
    {
        location.x = Config.PosX_ListNames;
        location.y = Config.PosX_ListNames;
    }

    if (ListNames.size() <= 0)
    {
        // Empty directories or zip files
        if (Config.UseZipSupport == true && Profile.ZipFile.length() > 0)
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

            rect_clip.w = Config.DisplayListMaxWidth-location.x;
            rect_clip.h = text_surface->h;
            rect_clip.x = 0;
            rect_clip.y = 0;

            ApplyImage( location.x, location.y,  text_surface, Screen, &rect_clip );

            location.x -= ImageSelectPointer->w;
            location.y += text_surface->h + Config.EntryYDelta;

            SDL_FreeSurface( text_surface );
            text_surface = NULL;
        }
        else
        {
            Log( "Failed to create TTF surface with TTF_RenderText_Solid: %s\n", TTF_GetError() );
            return 1;
        }
    }
    else
    {
        for (entry_index=0; entry_index<ListNames.size(); entry_index++)
        {
            offset = 0;

            // Draw the selector pointer
            if (entry_index == DisplayList.at(Mode).relative)
            {
                ApplyImage( location.x, location.y,  ImageSelectPointer, Screen, NULL );

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

            text_surface = TTF_RenderText_Solid( Fonts.at(ListNames.at(entry_index).font), ListNames.at(entry_index).text.c_str(), Config.Colors.at(ListNames.at(entry_index).color) );

            if (text_surface != NULL)
            {
                location.x += ImageSelectPointer->w;
                RectEntries.at(entry_index).x = location.x;
                RectEntries.at(entry_index).y = location.y;
                RectEntries.at(entry_index).w = text_surface->w;
                RectEntries.at(entry_index).h = text_surface->h;

                if (text_surface->w > (Config.DisplayListMaxWidth-location.x) )
                {
                    RectEntries.at(entry_index).w = Config.DisplayListMaxWidth-location.x;

                    if (Config.TextScrollOption == true && DisplayList.at(Mode).relative == entry_index)
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
                                    TextScrollOffset += Config.ScreenRatioW;
                                }
                                else
                                {
                                    TextScrollOffset -= Config.ScreenRatioW;
                                }
                                Redraw = true;
                            }

                            if (RectEntries.at(entry_index).w+TextScrollOffset >= text_surface->w)
                            {
                                TextScrollDir   = false;
                                CurScrollPause  = 2;
                            }
                            else if (TextScrollOffset <= 0)
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

                location.x -= ImageSelectPointer->w;
                location.y += text_surface->h + Config.EntryYDelta;

                SDL_FreeSurface( text_surface );
                text_surface = NULL;
            }
            else
            {
                Log( "Failed to create TTF surface with TTF_RenderText_Solid: %s\n", TTF_GetError() );
                return 1;
            }
        }
    }

    return 0;
}

void CSelector::SelectionLimits( item_pos_t& pos )
{
    if (pos.absolute <= pos.first)
    {
        pos.relative = 0;
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
        SDL_FillRect( Screen, NULL, rgb_to_int(Config.Colors.at(Config.ColorBackground)) );
    }
}

int8_t CSelector::ConfigureButtons( void )
{
    uint16_t i;

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
            if (Config.UseZipSupport == true && Profile.ZipFile.length() > 0)
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
            Log( "Error: Unknown Mode\n" );
            return 1;
            break;
    }

    // Overides for button driven by config options
    for (i=0; i<ButtonModesLeft.size(); i++ )
    {
        if (Config.ButtonModesLeftEnable.at(i) == false)
        {
            ButtonModesLeft.at(i) = EVENT_NONE;
        }
    }

    for (i=0; i<ButtonModesRight.size(); i++ )
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
    uint8_t button;
    SDL_Rect preview;

    if (Config.AutoLayout == false)
    {
        location.x = Config.PosX_ButtonLeft;
        location.y = Config.PosY_ButtonLeft;
    }

    // Draw buttons on left
    for (button=0; button<BUTTONS_MAX_LEFT; button++)
    {
        if (ButtonModesLeft.at(button) != EVENT_NONE)
        {
            RectButtonsLeft.at(button).x = location.x;
            RectButtonsLeft.at(button).y = location.y+(Config.ButtonHeightLeft*button)+(Config.EntryYOffset*button);
            RectButtonsLeft.at(button).w = Config.ButtonWidthLeft;
            RectButtonsLeft.at(button).h = Config.ButtonHeightLeft;

            if (DrawButton( ButtonModesLeft.at(button), Fonts.at(FONT_SIZE_LARGE), RectButtonsLeft.at(button) ))
            {
                return 1;
            }
        }
    }
    location.x += Config.ButtonWidthLeft + Config.EntryXOffset;

    // Draw buttons on right
    for (button=0; button<BUTTONS_MAX_RIGHT; button++)
    {
        if (ButtonModesRight.at(button) != EVENT_NONE)
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

            if (DrawButton( ButtonModesRight.at(button), Fonts.at(FONT_SIZE_LARGE), RectButtonsRight.at(button) ))
            {
                return 1;
            }
        }
    }

    //Display the preview graphic
    if (Mode == MODE_SELECT_ENTRY && ImagePreview != NULL)
    {
        preview.x = Config.ScreenWidth-Config.PreviewWidth-Config.EntryXOffset;
        preview.y = Config.ScreenHeight-Config.PreviewHeight-(Config.ButtonHeightRight*3)-(Config.EntryYOffset*6);
        //preview.w = Config.PreviewWidth;
        //preview.h = Config.PreviewHeight;

        ApplyImage( preview.x, preview.y, ImagePreview, Screen, NULL );
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
        SDL_FillRect( Screen, &location, rgb_to_int(Config.Colors.at(Config.ColorButton)) );
    }

    if (Config.ShowLabels == true)
    {
        text_surface = TTF_RenderText_Solid( font, LabelButtons.at(button).c_str(), Config.Colors.at(Config.ColorFontButton) );

        rect_text.x = location.x + ((location.w-text_surface->w)/2);
        rect_text.y = location.y + ((location.h-text_surface->h)/2);

        if (text_surface != NULL)
        {
            ApplyImage( rect_text.x, rect_text.y, text_surface, Screen, NULL );
            SDL_FreeSurface( text_surface );
            text_surface = NULL;
        }
        else
        {
            Log( "Failed to create TTF surface with TTF_RenderText_Solid: %s\n", TTF_GetError() );
            return 1;
        }
    }
    return 0;
}

int8_t CSelector::DrawText( SDL_Rect& location )
{
    int16_t         total;
    string          text;
    SDL_Rect        box, clip;
    SDL_Surface*    text_surface = NULL;

    if (Config.AutoLayout == false)
    {
        location.x = Config.PosX_Title;
        location.y = Config.PosY_Title;
    }

    // Title text
    text = string(APPNAME) + " " + string(APPVERSION);
    if (Profile.TargetApp.length() > 0)
    {
        text += " for " + Profile.TargetApp;
    }

    text_surface = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_LARGE), text.c_str(), Config.Colors.at(Config.ColorFontFiles) );
    if (text_surface != NULL)
    {
        ApplyImage( location.x, location.y, text_surface, Screen, NULL );
        location.y += text_surface->h + Config.EntryYDelta;

        SDL_FreeSurface( text_surface );
        text_surface = NULL;
    }
    else
    {
        Log( "Failed to create TTF surface with TTF_RenderText_Solid: %s\n", TTF_GetError() );
        return 1;
    }

    // Entry Filter
    if (Mode == MODE_SELECT_ENTRY && Profile.EntryFilter.length() > 0)
    {
        text_surface = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_MEDIUM), Profile.EntryFilter.c_str(), Config.Colors.at(Config.ColorFontFiles) );
        if (text_surface != NULL)
        {
            clip.x = 0;
            clip.y = 0;
            clip.h = text_surface->h;
            clip.w = Config.FilePathMaxWidth;
            if (text_surface->w > Config.FilePathMaxWidth)
            {
                clip.x = text_surface->w-Config.FilePathMaxWidth;
            }

            location.x = Config.ScreenWidth - text_surface->w - Config.EntryXOffset;

            ApplyImage( location.x, location.y, text_surface, Screen, &clip );

            location.x = Config.EntryXOffset;

            SDL_FreeSurface( text_surface );
            text_surface = NULL;
        }
        else
        {
            Log( "Failed to create TTF surface with TTF_RenderText_Solid: %s\n", TTF_GetError() );
            return 1;
        }
    }

    // File path
    if (Mode == MODE_SELECT_ENTRY)
    {
        text = Profile.FilePath;
        if (Profile.ZipFile.length())
        {
            text += "->" + Profile.ZipFile;
        }

        text_surface = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_MEDIUM), text.c_str(), Config.Colors.at(Config.ColorFontFiles) );
        if (text_surface != NULL)
        {
            clip.x = 0;
            clip.y = 0;
            clip.h = text_surface->h;
            clip.w = Config.FilePathMaxWidth;
            if (text_surface->w > Config.FilePathMaxWidth)
            {
                clip.x = text_surface->w-Config.FilePathMaxWidth;
            }

            ApplyImage( location.x, location.y, text_surface, Screen, &clip );

            location.y += text_surface->h + Config.EntryYOffset;

            SDL_FreeSurface( text_surface );
            text_surface = NULL;
        }
        else
        {
            Log( "Failed to create TTF surface with TTF_RenderText_Solid: %s\n", TTF_GetError() );
            return 1;
        }
    }

    // About text
    text = "Written by " + string(APPAUTHOR) + " " + string(APPCOPYRIGHT);
    text_surface = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_SMALL), text.c_str(), Config.Colors.at(Config.ColorFontFiles) );

    if (text_surface != NULL)
    {
        box.x = Config.ScreenWidth  - text_surface->w - Config.EntryXOffset;
        box.y = Config.ScreenHeight - text_surface->h - Config.EntryYOffset;

        ApplyImage( box.x, box.y, text_surface, Screen, NULL );
        SDL_FreeSurface( text_surface );
        text_surface = NULL;
    }
    else
    {
        Log( "Failed to create TTF surface with TTF_RenderText_Solid: %s\n", TTF_GetError() );
        return 1;
    }

    // Item count
    switch (Mode)
    {
        case MODE_SELECT_ENTRY:
            total = ItemsEntry.size();
            break;
        case MODE_SELECT_ARGUMENT: /* fall through */
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
    text = "0 of 0";
    if (total > 0)
    {
        text = i_to_a(DisplayList.at(Mode).absolute+1) + " of " + i_to_a(total);
    }
    text_surface = TTF_RenderText_Solid(Fonts.at(FONT_SIZE_SMALL), text.c_str(), Config.Colors.at(Config.ColorFontFiles));

    if (text_surface != NULL)
    {
        box.x = Config.EntryXOffset;
        box.y = Config.ScreenHeight - text_surface->h - Config.EntryYOffset;

        ApplyImage( box.x, box.y, text_surface, Screen, NULL );
        SDL_FreeSurface( text_surface );
        text_surface = NULL;
    }
    else
    {
        Log( "Failed to create TTF surface with TTF_RenderText_Solid: %s\n", TTF_GetError());
        return 1;
    }

    // Zip extract option
    if (Config.UseZipSupport == true && Profile.ZipFile.length() > 0)
    {
        if (ExtractAllFiles == true)
        {
            text = "Extract All";
        }
        else
        {
            text = "Extract Selection";
        }

        text_surface = TTF_RenderText_Solid(Fonts.at(FONT_SIZE_SMALL), text.c_str(), Config.Colors.at(Config.ColorFontFiles));

        if (text_surface != NULL)
        {
            box.x = 5*Config.EntryXOffset;
            box.y = Config.ScreenHeight - text_surface->h - Config.EntryYOffset;

            ApplyImage( box.x, box.y, text_surface, Screen, NULL );
            SDL_FreeSurface( text_surface );
            text_surface = NULL;
        }
        else
        {
            Log( "Failed to create TTF surface with TTF_RenderText_Solid: %s\n", TTF_GetError());
            return 1;
        }
    }

#if defined(DEBUG)
    text = "DEBUG abs " + i_to_a(DisplayList.at(Mode).absolute)  + " rel " + i_to_a(DisplayList.at(Mode).relative)
         + " 1st " + i_to_a(DisplayList.at(Mode).first) + " end " + i_to_a(DisplayList.at(Mode).last)
         + " tot " + i_to_a(DisplayList.at(Mode).total)
         + " fps " + i_to_a(FPSDrawn) + " skip " + i_to_a(FPSSkip);

    text_surface = TTF_RenderText_Solid( Fonts.at(FONT_SIZE_SMALL), text.c_str(), Config.Colors.at(Config.ColorFontFiles) );

    if (text_surface != NULL)
    {
        box.x = Config.EntryXOffset;
        box.y = Config.ScreenHeight - text_surface->h;

        ApplyImage( box.x, box.y, text_surface, Screen, NULL );
        SDL_FreeSurface( text_surface );
        text_surface = NULL;
    }
    else
    {
        Log( "Failed to create TTF surface with TTF_RenderText_Solid: %s\n", TTF_GetError() );
        return 1;
    }
#endif

    return 0;
}

int8_t CSelector::RunExec( uint16_t selection )
{
    bool entry_found;
    uint16_t i, j;
    int16_t ext_index;
    string filename;
    string filepath;
    string command;
    string extension;
    string value;
    entry_t* entry = NULL;
    argforce_t* argforce = NULL;
    argument_t* argument = NULL;

    // Find a entry for argument values
    entry_found = false;

    if (!CheckRange( selection, ItemsEntry.size() ))
    {
        Log( "Error: RunExec selection is out of range\n" );
        return 1;
    }

    if (ItemsEntry.at(selection).Entry >= 0)
    {
        entry = &Profile.Entries.at(ItemsEntry.at(selection).Entry);
        entry_found = true;
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

        // Setup commands
        for (i=0; i<Profile.Commands.size(); i++)
        {
            command += "cd " + Profile.Commands.at(i).Path + "; ";
            command += Profile.Commands.at(i).Command;

            for (j=0; j<Profile.Commands.at(i).Arguments.size(); j++)
            {
                if (Profile.Commands.at(i).Arguments.at(j).Flag.compare(VALUE_NOVALUE) != 0)
                {
                    command += " " + Profile.Commands.at(i).Arguments.at(j).Flag;
                }
                if (entry_found==true)
                {
                    command += " " + Profile.Commands.at(i).Arguments.at(j).Values.at(entry->CmdValues.at(j));
                }
                else
                {
                    command += " " + Profile.Commands.at(i).Arguments.at(j).Values.at(Profile.Commands.at(i).Arguments.at(j).Default);
                }
            }
            command += "; ";
        }

        // Setup executable
        command += "cd " + Profile.Extensions.at(ext_index).exePath + "; ";
        command += "LD_LIBRARY_PATH=./; export LD_LIBRARY_PATH; ";
        command += "./" + Profile.Extensions.at(ext_index).exeName;

        // Unzip if needed
        if (Config.UseZipSupport == true && Profile.ZipFile.length() > 0)
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

        // Setup arguments
        for (i=0; i<Profile.Extensions.at(ext_index).Arguments.size(); i++)
        {
            value.clear();

            argument = &Profile.Extensions.at(ext_index).Arguments.at(i);
            // Check arg forces
            for (j=0; j<Profile.Extensions.at(ext_index).ArgForces.size(); j++)
            {
                argforce = &Profile.Extensions.at(ext_index).ArgForces.at(j);
                if (argforce->Path.compare( Profile.FilePath ) == 0)
                {
                    if (i == argforce->Argument)
                    {
                        Log( "Setting argforce on arg %d\n", i );
                        value = argforce->Value;
                        break;
                    }
                }
            }
            // Check arguments for default value or custom entry value
            if (value.length() <= 0 )
            {
                if (entry_found==true)
                {
                    if (CheckRange( i, entry->ArgValues.size() ))
                    {
                        value = argument->Values.at( entry->ArgValues.at(i) );
                    }
                    else
                    {
                        Log( "Error: RunExec i is out of range\n" );
                        return 1;
                    }
                }
                else
                {
                    if (CheckRange( argument->Default, argument->Values.size() ))
                    {
                        value = argument->Values.at( argument->Default );
                    }
                    else
                    {
                        Log( "Error: RunExec argument->Default is out of range\n" );
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

                    if (value.compare( VALUE_FILENAME ) == 0)
                    {
                        if (Config.FilenameArgNoExt == true)
                        {
                            filename = filename.substr( 0, filename.find_last_of(".") );
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
    else
    {
        Log( "Warning no extension was found for this file type\n" );
        return 1;
    }

    command += "; sync;";
    if (Config.ReloadLauncher == true)
    {
        command += " cd " + Profile.LauncherPath + ";";
        command += " exec " + Profile.LauncherName;
    }

    Log( "Running command: '%s'\n", command.c_str());

    CloseResources(0);

    execlp( "/bin/sh", "/bin/sh","-c", command.c_str(), NULL );

    //if execution continues then something went wrong and as we already called SDL_Quit we cannot continue, try reloading
    Log( "Error executing selected application, re-launching %s\n", APPNAME);

    chdir( Profile.LauncherPath.c_str() );
    execlp( Profile.LauncherName.c_str(), Profile.LauncherName.c_str(), NULL );

    return 0;
}

int8_t CSelector::PollInputs( void )
{
    int16_t     newsel;
    uint16_t    index;
    string      keyname;
    SDL_Event   event;

    for (index=0; index<EventReleased.size(); index++)
    {
        if (EventReleased.at(index) == true)
        {
            EventReleased.at(index)     = false;
            EventPressCount.at(index)   = EVENT_LOOPS_OFF;
#if defined(DEBUG)
            //Log( "DEBUG EventReleased %d\n", index );
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
        Log( "Error: PollInputs Mode out of range\n" );
        return 1;
    }

    while (SDL_PollEvent( &event ))
    {
        switch( event.type )
        {
            case SDL_KEYDOWN:
                keyname = SDL_GetKeyName( event.key.keysym.sym );
                if (keyname.length() == 1 || event.key.keysym.sym == SDLK_BACKSPACE)
                {
                    if (Config.EntryFastMode == ENTRY_FAST_MODE_ALPHA)
                    {
                        DisplayList.at(Mode).absolute = Profile.AlphabeticIndices.at(event.key.keysym.sym-SDLK_a);
                        Profile.EntryFilter.clear();
                    }
                    else if (Config.EntryFastMode == ENTRY_FAST_MODE_FILTER)
                    {
                        if (event.key.keysym.sym==SDLK_BACKSPACE || event.key.keysym.sym==SDLK_DELETE)
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
                        Rescan = true;
                    }

                    RefreshList = true;
                }
                else
                {
                    for (index=0; index<EventPressCount.size(); index++)
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
                break;

            case SDL_KEYUP:
                for (index=0; index<EventReleased.size(); index++)
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
                break;

            case SDL_JOYBUTTONDOWN:
                for (index=0; index<EventPressCount.size(); index++)
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
                break;

            case SDL_JOYBUTTONUP:
                for (index=0; index<EventReleased.size(); index++)
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
                break;

            case SDL_MOUSEMOTION:
                Mouse.x  = event.motion.x;
                Mouse.y  = event.motion.y;

                for (index=0; index<RectEntries.size(); index++)
                {
                    if (CheckRectCollision( &Mouse, &RectEntries.at(index) ))
                    {
                        // Determine item being hovered over by the pointer
                        newsel = DisplayList.at(Mode).first+index;
                        // Only need to refresh the names when a new entry is hovered
                        if (newsel != DisplayList.at(Mode).absolute)
                        {
                            RefreshList = true;
                        }
                        DisplayList.at(Mode).absolute = newsel;
                        DisplayList.at(Mode).relative = index;
                        break;
                    }
                }

                Redraw = true;
                break;

            case SDL_MOUSEBUTTONDOWN:
                switch (event.button.button)
                {
                    case SDL_BUTTON_LEFT:
                    case SDL_BUTTON_MIDDLE:
                    case SDL_BUTTON_RIGHT:;
                        Mouse.x  = event.button.x;
                        Mouse.y  = event.button.y;
                        for (index=0; index<RectButtonsLeft.size(); index++)
                        {
                            if (ButtonModesLeft.at(index)!=EVENT_NONE)
                            {
                                if (CheckRectCollision( &Mouse, &RectButtonsLeft.at(index) ))
                                {
                                    EventPressCount.at(ButtonModesLeft.at(index)) = EVENT_LOOPS_ON;
#if defined(DEBUG)
                                    Log( "DEBUG LeftButton Active %d %d\n", ButtonModesLeft.at(index), index );
#endif
                                }
                            }
                        }
                        for (index=0; index<RectButtonsRight.size(); index++)
                        {
                            if (ButtonModesRight.at(index)!=EVENT_NONE)
                            {
                                if (CheckRectCollision( &Mouse, &RectButtonsRight.at(index) ))
                                {
                                    EventPressCount.at(ButtonModesRight.at(index)) = EVENT_LOOPS_ON;
#if defined(DEBUG)
                                    Log( "DEBUG RightButton Active %d %d\n", ButtonModesRight.at(index), index );
#endif
                                }
                            }
                        }
                        break;
                    case SDL_BUTTON_WHEELUP:
                        EventPressCount.at(EVENT_ONE_UP) = EVENT_LOOPS_ON;
                        break;
                    case SDL_BUTTON_WHEELDOWN:
                        EventPressCount.at(EVENT_ONE_DOWN) = EVENT_LOOPS_ON;
                        break;
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
                    case SDL_BUTTON_WHEELUP:
                    case SDL_BUTTON_WHEELDOWN:
                        Mouse.x  = event.button.x;
                        Mouse.y  = event.button.y;

                        for (index=0; index<EventReleased.size(); index++)
                        {
                            EventReleased.at(index) = true;
                        }
#if defined(DEBUG)
                        Log( "DEBUG Releasing all events\n" );
#endif
                        break;
                    default:
                        break;
                }
                break;
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
        if (Rescan == false && IsEventOn(EVENT_DIR_UP) == true)
        {
            if (Profile.ZipFile.length() > 0)
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
        if (Rescan == false && ((IsEventOn(EVENT_DIR_DOWN) == true) || (IsEventOn(EVENT_SELECT) == true)))
        {
            if (CheckRange( DisplayList.at(Mode).absolute, ItemsEntry.size() ))
            {
                if (ItemsEntry.at(DisplayList.at(Mode).absolute).Type == TYPE_DIR)
                {
                    if (Profile.LaunchableDirs == false)
                    {
                        DirectoryDown();
                    }
                }
                else if (ItemsEntry.at(DisplayList.at(Mode).absolute).Type == TYPE_ZIP)
                {
                    ZipDown();
                }
            }
            else
            {
                Log( "Error: PollInputs DisplayList.at(Mode).absolute out of range\n" );
                return 1;
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
            RefreshList = true;
            SetOneEntryValue = true;
        }
        if (IsEventOn(EVENT_SET_ALL) == true)
        {
            RefreshList = true;
            SetAllEntryValue = true;
        }
    }

    if (IsEventOn(EVENT_ZIP_MODE) == true)
    {
        ExtractAllFiles = !ExtractAllFiles;
    }

    return 0;
}
