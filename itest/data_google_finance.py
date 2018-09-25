#!/usr/bin/python3
import asyncio
import aiohttp
import argparse

URL = \
    'http://www.google.com/finance/getprices?' \
    'i={interval}&p={days}d&f=d,o,h,l,c,v&df=cpct&q={ticker}'


def _data_to_csv(data, ticker, interval):
    lines = []
    series = None
    ts = None
    tz_offset = None
    for n, line in enumerate(data.splitlines()):
        if line.startswith('COLUMNS='):
            series = [
                'GOOGLE-FINANCE-{}-{}'.format(ticker, column)
                for column in line[13:].split(',')]
            lines.append(',' + ','.join(series))
        elif line.startswith('TIMEZONE_OFFSET='):
            tz_offset = int(line[17:])
        elif tz_offset is not None:
            date, *fields = line.split(',')
            if date[0] == 'a':
                ts = int(date[1:])
                offset = 0
            else:
                offset = int(date) * interval
            lines.append('{},{:.2f},{:.2f},{:.2f},{:.2f},{}'.format(
                ts + offset, *map(float, fields[:-1]), fields[-1]))

    if series is None:
        raise Exception('Missing series in data')

    return '\n'.join(lines)


async def get_google_finance_data(ticker, interval, days):
    """Returns Google Finance Data in CSV format.

    :param ticker: This is the ticker symbol of the stock
    :param interval: Interval or frequency in seconds (minimal 60)
    :param days: The historical data period in days
    :return: string (csv format)
    """
    assert interval >= 60, \
        'Inteval lower than 60 is not supported by the Google Finance API.'

    async with aiohttp.ClientSession() as session:
        async with session.get(URL.format(
                interval=interval,
                days=days,
                ticker=ticker)) as resp:
            content = await resp.text()
            return _data_to_csv(content, ticker, interval)


async def save_google_finance_data(fn, ticker, interval, days):
    """Returns Google Finance Data in CSV format.

    :param fn: Filename where the csv data is saved.
    :param ticker: This is the ticker symbol of the stock
    :param interval: Interval or frequency in seconds (minimal 60)
    :param days: The historical data period in days
    :return: None
    """
    csv_data = await get_google_finance_data(ticker, interval, days)
    with open(fn, 'w') as f:
        f.write(csv_data)


async def print_google_finance_data(ticker, interval, days):
    """Prints Google Finance Data in CSV format.

    :param ticker: This is the ticker symbol of the stock
    :param interval: Interval or frequency in seconds (minimal 60)
    :param days: The historical data period in days
    :return: None
    """
    print(await get_google_finance_data(ticker, interval, days))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '-t', '--ticker',
        help='Ticker symbol of the stock',
        default='IBM',
        # required=True,
        type=str)

    parser.add_argument(
        '-i', '--interval',
        default=60,
        help='Interval or frequency in seconds',
        type=int)

    parser.add_argument(
        '-d', '--days',
        default=10,
        help='The historical data period in days',
        type=int)

    args = parser.parse_args()

    loop = asyncio.get_event_loop()
    loop.run_until_complete(print_google_finance_data(
        ticker=args.ticker,
        interval=args.interval,
        days=args.days))
