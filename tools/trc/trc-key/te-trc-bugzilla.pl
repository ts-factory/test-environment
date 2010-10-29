#!/usr/bin/perl
#
# Test Environment: Testing Results Comparator
#
# Oktetlabs bugzilla page parser script
#
# Copyright (C) 2006 Test Environment authors (see file AUTHORS
# in the root directory of the distribution).
#
# Test Environment is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# Test Environment is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA  02111-1307  USA
#
# Author Alexander Kukuta <Alexander.Kukuta@oktetlabs.ru>
#
# $Id: $
#

printf("<bugs>\n");

# Iterate through the list of Bug IDs
for my $bug_id (@ARGV)
{
    my %bug = ();

    my $bug_url = "https://www.oktetlabs.ru/bugzilla3/show_bug.cgi?id=$bug_id";
    my $field = ();
    my $label = ();
    my $assume = ();

    #curl -s -u : --negotiate "${BUZILLA_URL}/show_bug.cgi?id=${bug_id}"
    for my $line (`curl -s -u : --negotiate \"$bug_url\"`)
    {
        # Parse BUG ID field
        if ($line =~ m/name=\"id\" value=\"(\d*)\"/i)
        {
            #printf("Id: %s\n", $1);
            $bug{'id'} = $1;
        }

        # Parse BUG Summary
        if ($line =~ m/short_desc_nonedit_display\"\>([\w\s.,<>:\;\?\=\-\\_\/\"\(\)\'\{\}\[\]\#\@\!\~\$\%\^\&\*\|]*)/i)
        {
            #printf("Summary: %s\n", $1);
            $bug{'summary'} = $1;
        }

        # Parse assumptions if any
        if ($assume)
        {
            if ($line =~ m/\s*(\w*)\s*/i)
            {
                #printf("Assume %s=%s\n", $assume, $1);
                $bug{$assume} = $1;
                $assume=();
            }
        }

        # Parse BUG Status
        if ($line =~ m/static_bug_status\"\>([\w ]*)/i)
        {
            #printf("Status: %s\n", $1);
            $bug{'status'} = $1;
            # Assume the next line contains bug resolution
            $assume='resolution';
        }

        # Parse Selected options
        if ($line =~ m/(select|input) (id|name)=\"([\w\s]*)\"/i)
        {
            #printf("Found %s select id\n", $1);
            $field = $3;
        }

        if ($line =~ m/label for=\"([\w\s]*)\"/i)
        {
            #printf("Found %s label\n", $1);
            $label = $1;
        }

        # Parse label for construction, in case there are no selections
        # due to read-only access to bugzilla
        if (($line =~ m/<td>([\w\s]*)/i) or
            ($line =~ m/span title=\"([\w\s\,]*)\"/i) or
            ($line =~ m/<td colspan=\"2\">([\w\s\,]*)  $/i))
        {
            if ($label)
            {
                #printf("%s: %s\n", $label, $1);
                $bug{$label} = $1;
                chomp $bug{$label};
                $label = ();
            }
        }

        # Parse selections
        if (($line =~ m/option value=\"([\w\s]*)\"\s*selected/i) or
            ($line =~ m/option selected value=\"([\w\s]*)\"/i) or
            ($line =~ m/^\s*value=\"([\w\s\,]*)\"/i))
        {
            if ($field)
            {
                #printf("%s: %s\n", $field, $1);
                $bug{$field} = $1;
                $field = ();
            }
        }

        if ($line =~ m/href=\"mailto\:([\w\&\#\;\.]*)\" title=\"([\w\&\#\;\.]*)\" ><span class=\"fn\">([\w\s]*)/i)
        {
            if (!$bug{'assigned-to'})
            {
                $bug{'assigned-to'} = $3;
                $bug{'assigned-mail'} = $1;
            }
        }
    }

    # Check priority field is simple.
    # Split it to priority and severity if not.
    if ($bug{'priority'} =~ m/(P[\d]*) ([\w\s]*)/i)
    {
        $bug{'bug_severity'} = $2;
        $bug{'priority'} = $1;
    }

    # Update bug status, if not parsed yet
    if ((not $bug{'bug_status'}) and ($bug{'status'}))
    {
        $bug{'bug_status'} = $bug{'status'};
    }

    if ($bug{'resolution'})
    {
        $bug{'bug_status'} .= ' ' . $bug{'resolution'}
    }

    printf("<bug id=\"OL %d\"", $bug{'id'});
    printf(" url=\"%s\"", $bug{'url'});
    printf(" priority=\"%s %s\"", $bug{'priority'}, $bug{'severity'});
    printf(" status=\"%s\"", $bug{'bug_status'});
    printf(" assigned-to=\"%s\"", $bug{'assigned-to'});
    printf(" assigned-mail=\"%s\"", $bug{'assigned-mail'});
    printf(" project=\"%s\"", $bug{'product'});
    printf(" components=\"%s\"", $bug{'component'});
    printf(" tags=\"%s\"", $bug{'keywords'});
    printf("/>\n");
}

printf("</bugs>\n");

