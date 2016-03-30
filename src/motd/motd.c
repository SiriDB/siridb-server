/*
 * motd.c - Message of the day
 *
 *  Note: all quotes are found somewhere on the Internet.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 13-03-2016
 *
 */
#include <stdlib.h>


static const char * motd_messages[] = {
        "If God had intended Man to program, \n"
        "we'd be born with serial I/O ports.",

        "\"On the Internet, no one knows you're using Windows NT\"\n"
        "(Submitted by Ramiro Estrugo, restrugo@fateware.com)",

        "Your mail is being routed through Germany ...\n"
        "and they're censoring us.",

        "There's got to be more to life than compile-and-go.",

        "One person's error is another person's data.",

        "Nobody said computers were going to be polite.",

        "Little known fact about Middle Earth:\n"
        "The Hobbits had a very sophisticated computer network!\n\n"
        "It was a Tolkien Ring...",

        "You can do anything, but not everything.\n\n"
        "--David Allen",

        "Perfection is achieved, not when there is nothing more to add,\n"
        "but when there is nothing left to take away.\n\n"
        "--Antoine de Saint-Exupéry",

        "The richest man is not he who has the most,\n"
        "but he who needs the least.\n\n"
        "--Unknown Author",

        "You miss 100 percent of the shots you never take.\n\n"
        "--Wayne Gretzky",

        "Courage is not the absence of fear,\n"
        "but rather the judgment that something else is more "
        "important than fear.\n\n"
        "--Ambrose Redmoon",

        "Everyone is a genius at least once a year.\n"
        "The real geniuses simply have their bright ideas closer together.\n\n"
        "--Georg Christoph Lichtenberg",

        "Before I got married I had six theories about bringing up children;\n"
        "now I have six children and no theories.\n\n"
        "--John Wilmot",

        "Those who believe in telekinetics, raise my hand.\n\n"
        "--Kurt Vonnegut",

        "We've heard that a million monkeys at a million keyboards could "
        "produce the complete works of Shakespeare;\n"
        "now, thanks to the Internet, we know that is not true.\n\n"
        "--Robert Wilensky",

        "The person who reads too much and uses his brain too little will "
        "fall into lazy habits of thinking.\n\n"
        "--Albert Einstein",

        "Believe those who are seeking the truth. Doubt those who find it.\n\n"
        "--André Gide",

        "It is the mark of an educated mind to be able to entertain a thought "
        "without accepting it.\n\n"
        "--Aristotle",

        "Pure mathematics is, in its way, the poetry of logical ideas.\n\n"
        "--Albert Einstein",

        "Do not worry about your difficulties in Mathematics. "
        "I can assure you mine are still greater.\n\n"
        "--Albert Einstein",

        "While physics and mathematics may tell us how the universe began,\n"
        "they are not much use in predicting human behavior because there "
        "are far too many equations to solve.\n"
        "I'm no better than anyone else at understanding what makes people "
        "tick, particularly women.\n\n"
        "--Stephen Hawking",

        "'Obvious' is the most dangerous word in mathematics.\n\n"
        "--E. T. Bell"

};

#define MOTD_SIZE (sizeof (motd_messages) / sizeof (const char *))

const char * motd_get_random_msg(void)
{
    return motd_messages[rand() % MOTD_SIZE];
}
