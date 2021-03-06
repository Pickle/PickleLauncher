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

#include "cprofile.h"

CProfile::CProfile() : CBase(),
    LaunchableDirs      (false),
    LauncherPath        (""),
    LauncherName        (""),
    FilePath            (""),
    TargetApp           (""),
    ZipFile             (""),
    EntryFilter         (""),
    Commands            (),
    Extensions          (),
    Entries             (),
    AlphabeticIndices   (),
    Minizip             ()
{
    AlphabeticIndices.resize(TOTAL_LETTERS, 0);
}

CProfile::~CProfile()
{
}

int8_t CProfile::Load( const string& location, const string& delimiter )
{
    bool            readline;
    string          line;
    ifstream        fin;

    Log( "  from location %s\n", location.c_str() );
    fin.open(location.c_str(), ios_base::in);

    if (!fin)
    {
        Log( "Error: Failed to open profile\n" );
        return 1;
    }

    // Read in the profile
    if (fin.is_open())
    {
        readline = true;
        while (!fin.eof())
        {
            if (readline == true)
            {
                getline(fin,line);
            }

            if (line.length() > 0)
            {
                // Common options
                if (UnprefixString( TargetApp, line, PROFILE_TARGETAPP ) == true)
                {
                    readline = true;
                }
                else
                if (UnprefixString( FilePath, line, PROFILE_FILEPATH ) == true)
                {
                    CheckPath(FilePath);
                    readline = true;
                }
                else    // Commands
                if (line.at(0) == '<' && line.at(line.length()-1) == '>')
                {
                    if (LoadCmd(fin, line, delimiter))
                    {
                        Log( "Error: Loading command from %s\n", location.c_str() );
                        return 1;
                    }
                    readline = false;
                }
                else    // Extensions
                if (line.at(0) == '[' && line.at(line.length()-1) == ']')
                {
                    if (LoadExt(fin, line, delimiter))
                    {
                        Log( "Error: Loading extension from %s\n", location.c_str() );
                        return 1;
                    }
                    readline = false;
                }
                else    // Entries
                if (line.at(0) == '{' && line.at(line.length()-1) == '}')
                {
                    if (LoadEntry(fin, line, delimiter))
                    {
                        Log( "Error: Loading entry from %s\n", location.c_str() );
                        return 1;
                    }
                    readline = false;
                }
                else
                {
                    readline = true;
                }
            }
            else
            {
                readline = true;
            }
        }
        fin.close();
    }
    else
    {
        Log( "Error: Failed to open profile\n" );
        return 1;
    }

    // Sanity checks
    if (FilePath.length()<=0)
    {
        Log( "Error: file path was not read from profile\n" );
        return 1;
    }
    if (Extensions.size()<=0)
    {
        Log( "Error: no extensions were read from profile\n" );
        return 1;
    }

    return 0;
}

int8_t CProfile::LoadCmd( ifstream& fin, string& line, const string& delimiter )
{
    uint8_t         i;
    uint8_t         count;
    command_t       cmd;
    argument_t      arg;
    vector<string>  parts;

    // Command name
    cmd.Name = line.substr( 1, line.length()-2 );

    // Command location and script
    getline(fin,line);
    if (UnprefixString( line, line, PROFILE_CMDPATH ) == true)
    {
        cmd.Command = line.substr( line.find_last_of('/')+1 );
        cmd.Path    = line.substr( 0, line.find_last_of('/')+1 );
        CheckPath( cmd.Path );

        getline(fin,line);
    }
    else
    {
        Log( "Error: %s not found\n", PROFILE_CMDPATH );
        return 1;
    }

    // Command arguments and values
    count = 0;
    while (UnprefixString( line, line, PROFILE_CMDARG ) == true)
    {
        SplitString( delimiter, line, parts );

        if (parts.size() >= ARG_MIN_COUNT)
        {
            arg.Name    = parts.at(0);
            arg.Flag    = parts.at(1);
            arg.Default = a_to_i( parts.at(2) );

            arg.Names.clear();
            arg.Values.clear();
            for (i=3; i<parts.size(); i++)
            {
                arg.Names.push_back( parts.at(i) );
                i++;
                if (i<parts.size())
                {
                    arg.Values.push_back( parts.at(i) );
                }
                else
                {
                    Log( "Error: Uneven number of argument names to values\n line:'%s'\n", line.c_str() );
                    return 1;
                }
            }

            if (arg.Default >= arg.Values.size())
            {
                arg.Default = arg.Values.size()-1;
            }

            cmd.Arguments.push_back( arg );

            count++;
            getline( fin, line );
        }
        else
        {
            Log( "Error: Not enough argument parts detected\n line:'%s'\n", line.c_str() );
            return 1;
        }
    }

    if (count<1)
    {
        Log( "Error: %s not found at least once\n", PROFILE_CMDARG );
        return 1;
    }

    Commands.push_back(cmd);
    return 0;
}

int8_t CProfile::LoadExt( ifstream& fin, string& line, const string& delimiter )
{
    uint8_t         i;
    uint8_t         count;
    vector<string>  parts;
    extension_t     ext;
    argument_t      arg;
    argforce_t      argforce;
    exeforce_t      exeforce;
    string          extensions;

    // Extension names
    extensions = line.substr(1, line.length()-2);
    SplitString( delimiter, extensions, parts );
    ext.extName.clear();
    for (i=0; i<parts.size(); i++)
    {
        ext.extName.push_back( lowercase(parts.at(i)) );
    }

    if (ext.extName.size() == 0)
    {
        Log( "Error: no extensions detected\n" );
        return 1;
    }

    // Extension executable
    getline(fin,line);
    if (UnprefixString( line, line, PROFILE_EXEPATH ) == true)
    {
        ext.exeName = line.substr( line.find_last_of('/')+1 );
        ext.exePath = line.substr( 0, line.find_last_of('/')+1 );
        CheckPath( ext.exePath );

        getline(fin,line);
    }
    else
    {
        Log( "Error: %s not found\n", PROFILE_EXEPATH );
        return 1;
    }

    // Extension blacklist
    if (UnprefixString( line, line, PROFILE_BLACKLIST ) == true)
    {
        SplitString( delimiter, line, ext.Blacklist );

        getline(fin,line);
    }

    // Extension arguments
    count = 0;
    while (UnprefixString( line, line, PROFILE_EXTARG ) == true)
    {
        SplitString( delimiter, line, parts );

        if (parts.size() >= ARG_MIN_COUNT)
        {
            arg.Name    = parts.at(0);
            arg.Flag    = parts.at(1);
            arg.Default = a_to_i( parts.at(2) );

            arg.Names.clear();
            arg.Values.clear();
            for (i=3; i<parts.size(); i++)
            {
                arg.Names.push_back( parts.at(i) );
                i++;
                if (i<parts.size())
                {
                    arg.Values.push_back( parts.at(i) );
                }
                else
                {
                    Log( "Error: Uneven number of argument names to values\n line:'%s'\n", line.c_str() );
                    return 1;
                }
            }

            if (arg.Default >= arg.Values.size())
            {
                arg.Default = arg.Values.size()-1;
            }

            ext.Arguments.push_back(arg);

            count++;
            getline(fin,line);
        }
        else
        {
            Log( "Error: Not enough argument parts detected\n line:'%s'\n", line.c_str() );
            return 1;
        }
    }

    if (count<1)
    {
        Log( "Error: %s not found at least once\n", PROFILE_EXTARG );
        return 1;
    }

    // Extension argforces
    while (UnprefixString( line, line, PROFILE_ARGFORCE ) == true)
    {
        SplitString( delimiter, line, parts );

        if (parts.size() == ARGFORCE_COUNT)
        {
            argforce.Path = parts.at(0);
            CheckPath(argforce.Path);

            argforce.Argument   = a_to_i( parts.at(1) );
            argforce.Value      = parts.at(2);
            ext.ArgForces.push_back( argforce );
        }
        else
        {
            Log( "Error: %s wrong number of parts actual: %d expected: %s\n", PROFILE_ARGFORCE, parts.size(), ARGFORCE_COUNT );
            return 1;
        }
        getline( fin, line );
    }

    // Exe path forces
    while (UnprefixString( line, line, PROFILE_EXEFORCE ) == true)
    {
        SplitString( delimiter, line, parts );

        if (parts.size()>=EXEFORCE_COUNT)
        {
            exeforce.exeName = parts.at(0).substr( line.find_last_of('/')+1 );
            exeforce.exePath = parts.at(0).substr( 0, line.find_last_of('/')+1 );
            CheckPath( exeforce.exePath );

            exeforce.Files.clear();
            for (i=1; i<parts.size(); i++)
            {
                exeforce.Files.push_back( parts.at(i) );
            }
            ext.ExeForces.push_back( exeforce );
        }
        else
        {
            Log( "Error: %s wrong number of parts actual: %d expected: %s\n", PROFILE_EXEFORCE, parts.size(), EXEFORCE_COUNT );
            return 1;
        }
        getline( fin, line );
    }
    Extensions.push_back(ext);

    // Check for directory exe
    if (ext.exeName.length() > 0)
    {
        for (i=0; i<ext.extName.size(); i++)
        {
            if (CheckExtension( ext.extName.at(i), EXT_DIRS) >= 0)
            {
                LaunchableDirs = true;
            }
        }
    }

    return 0;
}

int8_t CProfile::LoadEntry( ifstream& fin, string& line, const string& delimiter )
{
    uint16_t            i;
    string::size_type   pos1,pos2;
    entry_t             entry;
    vector<string>      parts;

    // Extension name
    pos1 = line.find_last_of('/');
    pos2 = line.find_last_of(delimiter);

    if ((pos1 == string::npos) || (pos1 > pos2))
    {
        entry.Path  = "./"; // default path
        pos1 = 0;
    }
    else
    {
        entry.Path  = line.substr( 1, pos1 );
    }

    if (pos2 == string::npos)
    {
        Log( "Error: A delimiter was expected in entry: %s\n", line.c_str() );
        return 1;
    }

    entry.Name  = line.substr( pos1+1, pos2-pos1-1 );
    entry.Alias = line.substr( pos2+1, line.length()-pos2-2 );

#if defined(DEBUG)
    Log( "DEBUG: Path: '%s' Name: '%s' Alias '%s'\n", entry.Path.c_str(), entry.Name.c_str(), entry.Alias.c_str() );
#endif

    // Extension executable
    getline(fin,line);
    if (UnprefixString( line, line, PROFILE_ENTRY_CMDS ) == true)
    {
        SplitString( delimiter, line, parts );

        if (   (parts.size() <= 0)
            || (parts.at(0).compare(VALUE_NOVALUE) == 0)
           )
        {
            entry.Custom = false;
        }
        else
        {
            for (i=0; i<parts.size(); i++)
            {
                entry.CmdValues.push_back( a_to_i( parts.at(i) ) );
            }
            entry.Custom = true;
        }
    }
    else
    {
        Log( "Error: %s not found\n", PROFILE_ENTRY_CMDS );
        return 1;
    }

    getline(fin,line);
    if (UnprefixString( line, line, PROFILE_ENTRY_ARGS ) == true)
    {
        SplitString( delimiter, line, parts );

        if (   (parts.size() <= 0)
            || (parts.at(0).compare(VALUE_NOVALUE) == 0)
           )
        {
            if (entry.Custom == true)
            {
                Log( "Error: %s custom values are present but were not for %s\n", PROFILE_ENTRY_ARGS, PROFILE_ENTRY_CMDS );
            }
        }
        else
        {
            if (entry.Custom == false)
            {
                Log( "Error: %s custom values are present but were not for %s\n", PROFILE_ENTRY_CMDS,PROFILE_ENTRY_ARGS );
            }
            else
            {
                for (i=0; i<parts.size(); i++)
                {
                    entry.ArgValues.push_back( a_to_i( parts.at(i) ) );
                }
            }
        }
    }
    else
    {
        Log( "Error: %s not found\n", PROFILE_ENTRY_ARGS );
        return 1;
    }
    Entries.push_back( entry );

    return 0;
}

int16_t CProfile::AddEntry( listoption_t& argument, const string& name )
{
    uint16_t i, j;
    entry_t entry;

    entry.Name      = name;
    entry.Path      = FilePath;
    entry.Custom    = true;
    entry.CmdValues.clear();
    for (i=0; i<Commands.size(); i++)
    {
        for (j=0; j<Commands.at(i).Arguments.size(); j++)
        {
            entry.CmdValues.push_back(Commands.at(i).Arguments.at(j).Default);
        }
    }
    entry.ArgValues.clear();
    if (CheckRange( argument.Extension, Extensions.size() ))
    {
        for (i=0; i<Extensions.at(argument.Extension).Arguments.size(); i++)
        {
            entry.ArgValues.push_back(Extensions.at(argument.Extension).Arguments.at(i).Default);
        }
    }
    else
    {
        Log( "Warning: AddEntry argument.Extension out of range for Extensions\n" );
        return -1;
    }
    Entries.push_back(entry);

    return Entries.size()-1;
}

int8_t CProfile::Save( const string& location, const string& delimiter )
{
    uint16_t index, i, j;
    ofstream   fout;

    fout.open( location.c_str(), ios_base::trunc );

    if (!fout)
    {
        Log( "Failed to open profile\n" );
        return 1;
    }

    // Write out the profile
    if (fout.is_open())
    {
        fout << "# Global Settings" << endl;
        fout << PROFILE_TARGETAPP << TargetApp << endl;
        fout << PROFILE_FILEPATH << FilePath << endl;
        // Commands
        fout << endl << "# Command Settings" << endl;
        for (index=0; index<Commands.size(); index++)
        {
            fout << "<" << Commands.at(index).Name << ">" << endl;
            fout << PROFILE_CMDPATH << Commands.at(index).Path << Commands.at(index).Command << endl;
            // Arguments
            for (i=0; i<Commands.at(index).Arguments.size(); i++)
            {
                fout << PROFILE_CMDARG << Commands.at(index).Arguments.at(i).Name << delimiter
                                       << Commands.at(index).Arguments.at(i).Flag << delimiter
                                       << i_to_a(Commands.at(index).Arguments.at(i).Default);
                for (j=0; j<Commands.at(index).Arguments.at(i).Values.size(); j++)
                {
                    fout << delimiter << Commands.at(index).Arguments.at(i).Names.at(j);
                    fout << delimiter << Commands.at(index).Arguments.at(i).Values.at(j);
                }
                fout << endl;
            }
            fout << endl;
        }

        // Extensions
        fout << endl << "# Extension Settings" << endl;
        for (index=0; index<Extensions.size(); index++)
        {
            fout << "[";
            for ( i=0; i<Extensions.at(index).extName.size(); i++ )
            {
                if (i>0)
                {
                    fout << delimiter;
                }
                fout << Extensions.at(index).extName.at(i);
            }
            fout << "]" << endl;

            // Executables
            fout << PROFILE_EXEPATH + Extensions.at(index).exePath + Extensions.at(index).exeName << endl;

            // Blacklist
            if (Extensions.at(index).Blacklist.size()>0)
            {
                fout << PROFILE_BLACKLIST;
                for (i=0; i<Extensions.at(index).Blacklist.size(); i++)
                {
                    if (i>0)
                    {
                        fout << delimiter;
                    }
                    fout << Extensions.at(index).Blacklist.at(i);
                }
                fout << endl;
            }
            // Arguments
            for (i=0; i<Extensions.at(index).Arguments.size(); i++)
            {
                fout << PROFILE_EXTARG << Extensions.at(index).Arguments.at(i).Name << delimiter
                                       << Extensions.at(index).Arguments.at(i).Flag << delimiter
                                       << i_to_a(Extensions.at(index).Arguments.at(i).Default);
                for (j=0; j<Extensions.at(index).Arguments.at(i).Values.size(); j++)
                {
                    fout << delimiter << Extensions.at(index).Arguments.at(i).Names.at(j);
                    fout << delimiter << Extensions.at(index).Arguments.at(i).Values.at(j);
                }
                fout << endl;
            }
            // Argument forces
            for (i=0; i<Extensions.at(index).ArgForces.size(); i++)
            {
                fout << PROFILE_ARGFORCE << Extensions.at(index).ArgForces.at(i).Path
                                         << delimiter << i_to_a(Extensions.at(index).ArgForces.at(i).Argument)
                                         << delimiter << Extensions.at(index).ArgForces.at(i).Value << endl;
            }
            // Exe forces
            for (i=0; i<Extensions.at(index).ExeForces.size(); i++)
            {
                fout << PROFILE_EXEFORCE << Extensions.at(index).ExeForces.at(i).exePath
                                         << Extensions.at(index).ExeForces.at(i).exeName;
                for (j=0; j<Extensions.at(index).ExeForces.at(i).Files.size(); j++)
                {
                    fout << delimiter << Extensions.at(index).ExeForces.at(i).Files.at(j);
                }
                fout << endl;
            }
            fout << endl;
        }

        // Entries
        fout << endl << "# Custom Entries Settings" << endl;
        for (index=0; index<Entries.size(); index++)
        {
            // Entry path, name, and alias
            fout << "{" << Entries.at(index).Path << Entries.at(index).Name
                        << delimiter
                        << Entries.at(index).Alias << "}" << endl;

            // Entry command values
            fout << PROFILE_ENTRY_CMDS;
            if (Entries.at(index).Custom == true)
            {
                for (i=0; i<Entries.at(index).CmdValues.size(); i++)
                {
                    if (i>0)
                    {
                        fout << delimiter;
                    }
                    fout << Entries.at(index).CmdValues.at(i);
                }
            }
            else
            {
                fout << VALUE_NOVALUE;
            }
            fout << endl;

            // Entry argument values
            fout << PROFILE_ENTRY_ARGS;
            if (Entries.at(index).Custom == true)
            {
                for (i=0; i<Entries.at(index).ArgValues.size(); i++)
                {
                    if (i>0)
                    {
                        fout << delimiter;
                    }
                    fout << Entries.at(index).ArgValues.at(i);
                }
            }
            else
            {
                fout << VALUE_NOVALUE;
            }
            fout << endl << endl;
        }

        fout.close();
    }
    else
    {
        Log( "Failed to open profile\n" );
        return 1;
    }
    return 0;
}

int8_t CProfile::ScanEntry( listitem_t& item, vector<listoption_t>& items )
{
    int16_t ext_index;
    uint16_t i, j;
    listoption_t option;
    string name;
    string value;

    items.clear();

    // Find the extension
    if ((item.Type == TYPE_DIR) && (LaunchableDirs == true))
    {
        ext_index = FindExtension(EXT_DIRS);
    }
    else
    {
        ext_index = FindExtension(item.Name);
    }

    if (CheckRange( ext_index, Extensions.size()))
    {
        option.Extension = ext_index;

        // Scan for command arguments
        if (Commands.size() > 0)
        {
            for (i=0; i<Commands.size(); i++)
            {
                for (j=0; j<Commands.at(i).Arguments.size(); j++)
                {
                    option.Name = Commands.at(i).Name + " ";
                    if (Commands.at(i).Arguments.at(j).Flag.compare(VALUE_NOVALUE) != 0)
                    {
                        option.Name += Commands.at(i).Arguments.at(j).Flag + " ";
                    }

                    if (CheckRange( item.Entry, Entries.size() ))
                    {
                        if (Entries.at(item.Entry).CmdValues.size() > 0)
                        {
                            if (Entries.at(item.Entry).CmdValues.at(i) < Commands.at(i).Arguments.at(j).Names.size())
                            {
                                option.Name += Commands.at(i).Arguments.at(j).Names.at( Entries.at(item.Entry).CmdValues.at(i) );
                            }
                            else
                            {
                                option.Name += "Error: ScanEntry Entry out of range";
                            }
                        }
                    }
                    else
                    {
                        if (CheckRange( Commands.at(i).Arguments.at(j).Default, Commands.at(i).Arguments.at(j).Names.size() ))
                        {
                            name = Commands.at(i).Arguments.at(j).Names.at( Commands.at(i).Arguments.at(j).Default );

                            if (name.compare(VALUE_NOVALUE) != 0)
                            {
                                option.Name += name;
                            }
                        }
                        else
                        {
                            option.Name += "Error: ScanEntry Default out of range";
                        }
                    }
                    option.Command = i;
                    option.Argument = j;
                    items.push_back(option);
                }
            }
        }

        // Scan for extension arguments
        if (Extensions.size() > 0)
        {
            for (i=0; i<Extensions.at(ext_index).Arguments.size(); i++)
            {
                option.Name = Extensions.at(ext_index).Arguments.at(i).Name + ": " +
                              Extensions.at(ext_index).Arguments.at(i).Flag + " ";

                if (CheckRange( item.Entry, Entries.size() ))
                {
                    if (i > Entries.at(item.Entry).ArgValues.size()-1)
                    {
                        Log( "Warning: ScanEntry the total arguments does not match the total values for the entry\n" );
                        Log( "Warning: Adding an entry at 0. You should check the order of the arguments.\n" );
                        Entries.at(item.Entry).ArgValues.push_back(0);
                    }

                    if (   (Extensions.at(ext_index).Arguments.at(i).Names.size() > 0)
                        && (Extensions.at(ext_index).Arguments.at(i).Values.size() > 0)
                       )
                    {
                        if (   CheckRange( Entries.at(item.Entry).ArgValues.at(i), Extensions.at(ext_index).Arguments.at(i).Names.size())
                            && CheckRange( Entries.at(item.Entry).ArgValues.at(i), Extensions.at(ext_index).Arguments.at(i).Values.size())
                           )
                        {
                            name = Extensions.at(ext_index).Arguments.at(i).Names.at( Entries.at(item.Entry).ArgValues.at(i) );

                            if (name.compare(VALUE_NOVALUE) != 0)
                            {
                                option.Name += name + " ";
                            }

                            value = Extensions.at(ext_index).Arguments.at(i).Values.at( Entries.at(item.Entry).ArgValues.at(i) );

                            if (value.compare(VALUE_NOVALUE) != 0)
                            {
                                option.Name += value;
                            }
                        }
                        else
                        {
                            option.Name += "Error: bad custom entry";
                        }
                    }
                    else
                    {
                        option.Name += "No Values";
                    }
                }
                else
                {
                    if (CheckRange( Extensions.at(ext_index).Arguments.at(i).Default, Extensions.at(ext_index).Arguments.at(i).Names.size()) &&
                        CheckRange( Extensions.at(ext_index).Arguments.at(i).Default, Extensions.at(ext_index).Arguments.at(i).Values.size())    )
                    {
                        name = Extensions.at(ext_index).Arguments.at(i).Names.at( Extensions.at(ext_index).Arguments.at(i).Default );

                        if (name.compare(VALUE_NOVALUE) != 0)
                        {
                            option.Name += name + " ";
                        }

                        value = Extensions.at(ext_index).Arguments.at(i).Values.at( Extensions.at(ext_index).Arguments.at(i).Default );

                        if (value.compare(VALUE_NOVALUE) != 0)
                        {
                            option.Name += value;
                        }
                    }
                    else
                    {
                        option.Name += "Error: index not found";
                    }
                }
                option.Command = -1;
                option.Argument = i;
                items.push_back(option);
            }
        }
    }
    else
    {
        Log( "Error ScanEntry ext_index out of range\n" );
        return -1;
    }
    return ext_index;
}

void CProfile::ScanArgument( listoption_t& item, vector<string>& values )
{
    uint16_t index;
    string value;

    values.clear();

    if (CheckRange( item.Command, Commands.size() ))
    {
        if (CheckRange(item.Argument, Commands.at(item.Command).Arguments.size() ))
        {
            for (index=0; index<Commands.at(item.Command).Arguments.at(item.Argument).Names.size(); index++)
            {
                value = Commands.at(item.Command).Arguments.at(item.Argument).Names.at(index);
                if (value.compare(VALUE_NOVALUE) == 0)
                {
                    values.push_back( "Not Active" );
                }
                else
                {
                    values.push_back( value );
                }
            }
        }
        else
        {
            Log( "Error: ScanArgument Commands item.Argument out of range\n" );
            values.push_back( "Error: Check log" );
        }
    }
    else if (CheckRange( item.Extension, Extensions.size() ))
    {
        if (CheckRange( item.Argument, Extensions.at(item.Extension).Arguments.size() ))
        {
            for (index=0; index<Extensions.at(item.Extension).Arguments.at(item.Argument).Names.size(); index++)
            {
                value = Extensions.at(item.Extension).Arguments.at(item.Argument).Names.at(index);
                if (value.compare(VALUE_NOVALUE) == 0)
                {
                    values.push_back( "Not Active" );
                }
                else
                {
                    values.push_back( value );
                }
            }
        }
        else
        {
            Log( "Error: ScanArgument Extensions item.Argument out of range\n" );
            values.push_back( "Error: Check log" );
        }
    }
    else
    {
        Log( "Error: ScanArgument item type undefined\n" );
        values.push_back( "Error: Check log" );
    }
}

int8_t CProfile::ScanDir( string location, bool showhidden, bool showzip, vector<listitem_t>& items )
{
    DIR *dp;
    struct dirent *dirp;
    string filename;
    bool found;
    int16_t alpha_index;
    int16_t ext_index;
    uint16_t file, i;
    listitem_t item;
    entry_t entry;
    vector<string> dirs;
    vector<string> files;
    vector<listitem_t>::iterator sort_index;

    dirs.clear();
    files.clear();
    items.clear();

    if (ZipFile.length() == 0)
    {
        if((dp = opendir(location.c_str())) == NULL)
        {
            Log( "Failed to open dir path %s\n", location.c_str() );
            return 1;
        }

        while ((dirp = readdir(dp)) != NULL)
        {
            filename = string(dirp->d_name);

            if (filename.length() > 0)
            {
                // Skip . and ..
                if (filename.compare(".") == 0 || filename.compare("..") == 0)
                    continue;

                // Skip hidden files and folders
                if (showhidden == false && filename.at(0) == '.')
                    continue;

                item.Entry  = -1;
                item.Name   = filename;
                if (dirp->d_type == DT_DIR)       // Directories
                {
                    // Filter out by blacklist
                    ext_index = FindExtension( EXT_DIRS );
                    found = false;
                    if (CheckRange( ext_index, Extensions.size() ))
                    {
                        for (i=0; i<Extensions.at(ext_index).Blacklist.size(); i++)
                        {
                            if (Extensions.at(ext_index).Blacklist.at(i).compare(filename) == 0)
                            {
                                found = true;
                                break;
                            }
                        }
                    }

                    if (found == false)
                    {
                        item.Type   = TYPE_DIR;
                        items.push_back(item);
                    }
                }
                else // Files
                {
                    files.push_back(filename);
                }
            }
            else
            {
                Log( "Error: filename length was 0\n" );
            }
        }
        closedir(dp);
    }
    else
    {
        Minizip.ListFiles( FilePath + ZipFile, files );
    }

    // Filter
    for (file=0; file<files.size(); file++)
    {
        found = false;
        if (CheckExtension( files.at(file), ZIP_EXT) < 0 ||                       // Any non-zip ext should be filtered
           (CheckExtension( files.at(file), ZIP_EXT) >= 0 && showzip == false))   // only filter zip if internal support is off
        {
            // Filter out files by extension
            ext_index = FindExtension( files.at(file) );

            // Filter out by blacklist
            found = false;
            if (CheckRange( ext_index, Extensions.size() ))
            {
                for (i=0; i<Extensions.at(ext_index).Blacklist.size(); i++)
                {
                    if (Extensions.at(ext_index).Blacklist.at(i).compare(files.at(file)) == 0)
                    {
                        found = true;
                        break;
                    }
                }
            }
            else
            {
                found = true;
            }
        }

        // Filter by search string
        if (found != true && EntryFilter.length() > 0)
        {
            if (lowercase(files.at(file)).find( lowercase(EntryFilter), 0) != string::npos)
            {
                found = false;
            }
            else
            {
                found = true;
            }
        }

        // If here then item is valid and determine if an entry exists
        if (found == false)
        {
            // Add to display list
            item.Name = files.at(file);
            // Check for zip file
            if (CheckExtension( files.at(file), ZIP_EXT) >= 0)
            {
                item.Type = TYPE_ZIP;
            }
            else
            {
                item.Type = TYPE_FILE;
            }

            // Find if an entry has been defined
            item.Entry = -1;
            for (i=0; i<Entries.size(); i++)
            {
                if (  (Entries.at(i).Name.compare(files.at(file)) == 0)
                    && (   (Entries.at(i).Path.compare("./") == 0)
                        || (Entries.at(i).Path.compare(location) == 0)
                       )
                   )
                {

                    item.Entry = i;
                    break;
                }
            }

            items.push_back(item);
        }
    }

    // Sort
    sort( items.begin(), items.end(), CompareItems );

    // Build alphabetic indices
    AlphabeticIndices.clear();
    AlphabeticIndices.resize(TOTAL_LETTERS, 0);
    for (i=0; i<items.size(); i++)
    {
        if (items.at(i).Type != TYPE_DIR)
        {
            if (items.at(i).Name.length() > 0 )
            {
                alpha_index = tolower(items.at(i).Name.at(0))-'a';
                if (alpha_index < 'a' || alpha_index > 'z')
                {
                    alpha_index = TOTAL_LETTERS-1;
                }

                if (CheckRange( alpha_index, AlphabeticIndices.size() ))
                {
                    if (AlphabeticIndices.at(alpha_index) == 0)
                    {
                        AlphabeticIndices.at(alpha_index) = i;
                    }
                }
                else
                {
                    Log( "Error: Scandir alpha_index out of range\n" );
                    return 1;
                }
            }
        }
    }
    alpha_index = 0;
    for (i=0; i<AlphabeticIndices.size(); i++)
    {
        if (AlphabeticIndices.at(i) == 0)
        {
            AlphabeticIndices.at(i) = alpha_index;
        }
        alpha_index = AlphabeticIndices.at(i);
    }
    return 0;
}

int16_t CProfile::FindExtension( const string& ext )
{
    uint16_t i, j;

    for (i=0; i<Extensions.size(); i++)
    {
        for (j=0; j<Extensions.at(i).extName.size(); j++)
        {
            if (CheckExtension( ext, Extensions.at(i).extName.at(j)) >= 0)
            {
                return i;
            }
        }
    }

    return -1;
}

// decide is a > b
bool CompareItems( listitem_t a, listitem_t b )
{
    // Folders should be above files
    if (a.Type == TYPE_DIR && b.Type >= TYPE_FILE)
    {
        return true;
    }
    else if (a.Type >= TYPE_FILE && b.Type == TYPE_DIR)
    {
        return false;
    }
    else
    {
        // Convert to lower cases, so that upper case files are sorted with lower case files
        transform( a.Name.begin(), a.Name.end(), a.Name.begin(), (int (*)(int))tolower );
        transform( b.Name.begin(), b.Name.end(), b.Name.begin(), (int (*)(int))tolower );

        if (a.Name.compare(b.Name) >= 0)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
}
