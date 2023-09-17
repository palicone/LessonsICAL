#include <string_view>
#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <variant>
#include <array>
#include <iostream>
#include <iterator>
#include <algorithm>

using namespace std::literals;

static void s_printUsage(std::string_view svExecutable)
{
  std::cout << "Usage:"sv << std::endl;
  std::cout << svExecutable << ' ' << "<-D=<first date>:<week day>:<begin-end[;begin-end]...:<lesson[,lesson]...>:<last date>... | filepath> [-PFX=<name>]"sv;
  std::cout << std::endl;
}

struct SParams
{
  std::string_view m_svPrefix;
  std::string_view m_svInputPath;
  std::vector<std::string> m_vstrDays;
};

struct SConfig
{
  struct SPeriod
  {
    std::string m_strStartTime;
    std::string m_strEndTime;
  };

  struct SLesson
  {
    std::string m_strName;
  };

  struct SDay
  {
    std::string m_strStartDate;
    std::string m_strDayOfWeek;
    std::vector<SPeriod> m_vPeriods;
    std::string m_strEndDate;
    std::vector<SLesson> m_vLessons;
  };

  std::vector<SDay> m_vDays;
};

static const SConfig::SPeriod& sc_getPeriod(const SConfig& rFromConfig, size_t cForDayIndex, size_t cForLessonIndex)
{
  if ( rFromConfig.m_vDays[cForDayIndex].m_vPeriods.size() > 0 )
  {
    return rFromConfig.m_vDays[cForDayIndex].m_vPeriods[cForLessonIndex];
  }
  return sc_getPeriod(rFromConfig, cForDayIndex - 1, cForLessonIndex);
}

static const std::string sc_getRepeatEndDate(const SConfig& rFromConfig, size_t cForDayIndex)
{
  if ( rFromConfig.m_vDays[cForDayIndex].m_strEndDate.size() > 0 )
  {
    return rFromConfig.m_vDays[cForDayIndex].m_strEndDate;
  }
  return sc_getRepeatEndDate(rFromConfig, cForDayIndex - 1);
}


static const std::string_view sc_svDefaultCalendarFormat =
R"(BEGIN:VCALENDAR
PRODID:-//palic.si//URNIK//EN
VERSION:2.0
CALSCALE:GREGORIAN
METHOD:PUBLISH
<LESSONS>
END:VCALENDAR)"sv;

std::string_view sc_svDefaultLessonFormat =
R"(BEGIN:VEVENT
DTSTART;TZID=Europe/Belgrade:<1ST_LESSON_DATE>T<LESSON_START_TIME>
DTEND;TZID=Europe/Belgrade:<1ST_LESSON_DATE>T<LESSON_END_TIME>
RRULE:FREQ=WEEKLY;WKST=<RPTDAY>;UNTIL=<RPTENDDATE>T215959Z;BYDAY=<RPTDAY>
SUMMARY:<WHO - ><LESSON_NAME>
END:VEVENT
)"sv;

int main(int argc, char** argv)
{
  SParams params;
  for ( int ARG = 1; ARG < argc; ++ARG )
  {
    std::string_view svArg = argv[ARG];
    if ( svArg.starts_with("-PFX="sv) )
    {
      params.m_svPrefix = svArg.substr(5);
      continue;
    }
    if ( svArg.starts_with("-D="sv) )
    {
      params.m_vstrDays.push_back(std::string{svArg.substr(3)});
      continue;
    }
    if ( std::filesystem::exists(svArg) )
    {
      params.m_svInputPath = svArg;
      continue;
    }
    std::cout << "Bad argument: " << svArg << std::endl << std::endl;
    s_printUsage(argv[0]);
    return -1;
  }

  if ( params.m_svInputPath.length() > 0 )
  {
    std::ifstream fInFile(std::filesystem::path{params.m_svInputPath});
    std::string strLine;
    while ( std::getline(fInFile, strLine) )
    {
      params.m_vstrDays.push_back(strLine);
    }
  }

  SConfig conf;

  for ( std::string_view svDay : params.m_vstrDays )
  {
    SConfig::SDay day;

    std::ranges::for_each(std::ranges::views::split(svDay, ':')
      | std::ranges::views::drop_while([&](const auto& rElem) { day.m_strStartDate = std::string_view{ rElem }; return false; }) | std::ranges::views::drop(1)
      | std::ranges::views::drop_while([&](const auto& rElem) { day.m_strDayOfWeek = std::string_view{ rElem }; return false; }) | std::ranges::views::drop(1)
      | std::ranges::views::drop_while([&](const auto& rElem)
        {
          std::ranges::for_each(std::string_view{ rElem } | std::ranges::views::split(','), [&](const auto& rPeriod)
            {
              SConfig::SPeriod period;
              period.m_strStartTime = std::string_view{ rPeriod };
              auto delimiter = period.m_strStartTime.find('-');
              period.m_strEndTime = period.m_strStartTime.substr(delimiter + 1);
              period.m_strStartTime.erase(delimiter);
              day.m_vPeriods.push_back(period);
            });
          return false;
        }) | std::ranges::views::drop(1)
      | std::ranges::views::drop_while([&](const auto& rElem) { day.m_strEndDate = std::string_view{ rElem }; return false; }) | std::ranges::views::drop(1)
      | std::ranges::views::drop_while([&](const auto& rElem)
        {
          std::ranges::for_each(std::string_view{ rElem } | std::ranges::views::split(','), [&](const auto& rLesson)
            {
              SConfig::SLesson lesson;
              lesson.m_strName = std::string_view{ rLesson };
              day.m_vLessons.push_back(lesson);
            });
          return false;
        }), [](...) {});
    conf.m_vDays.push_back(day);
  }

  std::string strLessons;
  std::string strLessonTemplate(sc_svDefaultLessonFormat);
  for ( size_t DAY = 0; DAY < conf.m_vDays.size(); ++DAY )
  {
    const SConfig::SDay& rDay = conf.m_vDays[DAY];
    for ( size_t LESSON = 0; LESSON < rDay.m_vLessons.size(); ++LESSON )
    {
      const SConfig::SLesson& rLesson = rDay.m_vLessons[LESSON];
      const SConfig::SPeriod& rPeriod = sc_getPeriod(conf, DAY, LESSON);
      std::string strLesson = strLessonTemplate;
      size_t pos;
      while ( pos = strLesson.find("<1ST_LESSON_DATE>"), pos != std::string::npos ) { strLesson = strLesson.replace(pos, 17, rDay.m_strStartDate); }
      while ( pos = strLesson.find("<LESSON_START_TIME>"), pos != std::string::npos ) { strLesson = strLesson.replace(pos, 19, rPeriod.m_strStartTime); }
      while ( pos = strLesson.find("<LESSON_END_TIME>"), pos != std::string::npos ) { strLesson = strLesson.replace(pos, 17, rPeriod.m_strEndTime); }
      while ( pos = strLesson.find("<RPTDAY>"), pos != std::string::npos ) { strLesson = strLesson.replace(pos, 8, rDay.m_strDayOfWeek); }
      while ( pos = strLesson.find("<RPTENDDATE>"), pos != std::string::npos ) { strLesson = strLesson.replace(pos, 12, sc_getRepeatEndDate(conf, DAY)); }
      if ( params.m_svPrefix.size() > 0 )
      {
        while ( pos = strLesson.find("<WHO - >"), pos != std::string::npos ) { strLesson = strLesson.replace(pos, 8, std::string(params.m_svPrefix).append(" - ")); }
      }
      else
      {
        while ( pos = strLesson.find("<WHO - >"), pos != std::string::npos ) { strLesson = strLesson.replace(pos, 8, ""); }
      }
      while ( pos = strLesson.find("<LESSON_NAME>"), pos != std::string::npos ) { strLesson = strLesson.replace(pos, 13, rLesson.m_strName); }

      strLessons = strLessons + strLesson;
    }
  }
  std::string strCal(sc_svDefaultCalendarFormat);
  strCal = strCal.replace(strCal.find("<LESSONS>"), 9, strLessons);
  std::cout << strCal << std::endl;

  return 0;
}
