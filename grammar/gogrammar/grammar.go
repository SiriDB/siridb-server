package grammar

// This grammar is generated using the Grammar.export_go() method and
// should be used with the goleri module.
//
// Source class: SiriGrammar
// Created at: 2017-03-13 21:42:33

import (
	"regexp"

	"github.com/transceptor-technology/goleri"
)

// Element identifiers
const (
	NoGid = iota
	GidAccessExpr = iota
	GidAccessKeywords = iota
	GidAfterExpr = iota
	GidAggregateFunctions = iota
	GidAlterDatabase = iota
	GidAlterGroup = iota
	GidAlterServer = iota
	GidAlterServers = iota
	GidAlterStmt = iota
	GidAlterUser = iota
	GidBeforeExpr = iota
	GidBetweenExpr = iota
	GidBoolOperator = iota
	GidBoolean = iota
	GidCDifference = iota
	GidCalcStmt = iota
	GidCountGroups = iota
	GidCountPools = iota
	GidCountSeries = iota
	GidCountSeriesLength = iota
	GidCountServers = iota
	GidCountServersReceived = iota
	GidCountShards = iota
	GidCountShardsSize = iota
	GidCountStmt = iota
	GidCountUsers = iota
	GidCreateGroup = iota
	GidCreateStmt = iota
	GidCreateUser = iota
	GidDropGroup = iota
	GidDropSeries = iota
	GidDropServer = iota
	GidDropShards = iota
	GidDropStmt = iota
	GidDropUser = iota
	GidFCount = iota
	GidFDerivative = iota
	GidFDifference = iota
	GidFFilter = iota
	GidFLimit = iota
	GidFMax = iota
	GidFMean = iota
	GidFMedian = iota
	GidFMedianHigh = iota
	GidFMedianLow = iota
	GidFMin = iota
	GidFPoints = iota
	GidFPvariance = iota
	GidFSum = iota
	GidFVariance = iota
	GidGrantStmt = iota
	GidGrantUser = iota
	GidGroupColumns = iota
	GidGroupMatch = iota
	GidGroupName = iota
	GidHelp = iota
	GidHelpAccess = iota
	GidHelpAlter = iota
	GidHelpAlterDatabase = iota
	GidHelpAlterGroup = iota
	GidHelpAlterServer = iota
	GidHelpAlterServers = iota
	GidHelpAlterUser = iota
	GidHelpCount = iota
	GidHelpCountGroups = iota
	GidHelpCountPools = iota
	GidHelpCountSeries = iota
	GidHelpCountServers = iota
	GidHelpCountShards = iota
	GidHelpCountUsers = iota
	GidHelpCreate = iota
	GidHelpCreateGroup = iota
	GidHelpCreateUser = iota
	GidHelpDrop = iota
	GidHelpDropGroup = iota
	GidHelpDropSeries = iota
	GidHelpDropServer = iota
	GidHelpDropShards = iota
	GidHelpDropUser = iota
	GidHelpFunctions = iota
	GidHelpGrant = iota
	GidHelpList = iota
	GidHelpListGroups = iota
	GidHelpListPools = iota
	GidHelpListSeries = iota
	GidHelpListServers = iota
	GidHelpListShards = iota
	GidHelpListUsers = iota
	GidHelpNoaccess = iota
	GidHelpRevoke = iota
	GidHelpSelect = iota
	GidHelpShow = iota
	GidHelpTimeit = iota
	GidHelpTimezones = iota
	GidIntExpr = iota
	GidIntOperator = iota
	GidKAccess = iota
	GidKActiveHandles = iota
	GidKAddress = iota
	GidKAfter = iota
	GidKAlter = iota
	GidKAnd = iota
	GidKAs = iota
	GidKBackupMode = iota
	GidKBefore = iota
	GidKBetween = iota
	GidKBufferPath = iota
	GidKBufferSize = iota
	GidKCount = iota
	GidKCreate = iota
	GidKCritical = iota
	GidKDatabase = iota
	GidKDbname = iota
	GidKDbpath = iota
	GidKDebug = iota
	GidKDerivative = iota
	GidKDifference = iota
	GidKDrop = iota
	GidKDropThreshold = iota
	GidKDurationLog = iota
	GidKDurationNum = iota
	GidKEnd = iota
	GidKError = iota
	GidKExpression = iota
	GidKFalse = iota
	GidKFilter = iota
	GidKFloat = iota
	GidKFor = iota
	GidKFrom = iota
	GidKFull = iota
	GidKGrant = iota
	GidKGroup = iota
	GidKGroups = iota
	GidKHelp = iota
	GidKIgnoreThreshold = iota
	GidKInfo = iota
	GidKInsert = iota
	GidKInteger = iota
	GidKIntersection = iota
	GidKIpSupport = iota
	GidKLength = iota
	GidKLibuv = iota
	GidKLimit = iota
	GidKList = iota
	GidKLog = iota
	GidKLogLevel = iota
	GidKMax = iota
	GidKMaxOpenFiles = iota
	GidKMean = iota
	GidKMedian = iota
	GidKMedianHigh = iota
	GidKMedianLow = iota
	GidKMemUsage = iota
	GidKMerge = iota
	GidKMin = iota
	GidKModify = iota
	GidKName = iota
	GidKNow = iota
	GidKNumber = iota
	GidKOnline = iota
	GidKOpenFiles = iota
	GidKOr = iota
	GidKPassword = iota
	GidKPoints = iota
	GidKPool = iota
	GidKPools = iota
	GidKPort = iota
	GidKPrefix = iota
	GidKPvariance = iota
	GidKRead = iota
	GidKReceivedPoints = iota
	GidKReindexProgress = iota
	GidKRevoke = iota
	GidKSelect = iota
	GidKSeries = iota
	GidKServer = iota
	GidKServers = iota
	GidKSet = iota
	GidKShards = iota
	GidKShow = iota
	GidKSid = iota
	GidKSize = iota
	GidKStart = iota
	GidKStartupTime = iota
	GidKStatus = iota
	GidKString = iota
	GidKSuffix = iota
	GidKSum = iota
	GidKSymmetricDifference = iota
	GidKSyncProgress = iota
	GidKTimePrecision = iota
	GidKTimeit = iota
	GidKTimezone = iota
	GidKTo = iota
	GidKTrue = iota
	GidKType = iota
	GidKUnion = iota
	GidKUptime = iota
	GidKUser = iota
	GidKUsers = iota
	GidKUsing = iota
	GidKUuid = iota
	GidKVariance = iota
	GidKVersion = iota
	GidKWarning = iota
	GidKWhere = iota
	GidKWhoAmI = iota
	GidKWrite = iota
	GidLimitExpr = iota
	GidListGroups = iota
	GidListPools = iota
	GidListSeries = iota
	GidListServers = iota
	GidListShards = iota
	GidListStmt = iota
	GidListUsers = iota
	GidLogKeywords = iota
	GidMergeAs = iota
	GidPoolColumns = iota
	GidPoolProps = iota
	GidPrefixExpr = iota
	GidRComment = iota
	GidRDoubleqStr = iota
	GidRFloat = iota
	GidRGraveStr = iota
	GidRInteger = iota
	GidRRegex = iota
	GidRSingleqStr = iota
	GidRTimeStr = iota
	GidRUinteger = iota
	GidRUuidStr = iota
	GidRevokeStmt = iota
	GidRevokeUser = iota
	GidSTART = iota
	GidSelectAggregate = iota
	GidSelectStmt = iota
	GidSeriesColumns = iota
	GidSeriesMatch = iota
	GidSeriesName = iota
	GidSeriesRe = iota
	GidSeriesSep = iota
	GidServerColumns = iota
	GidSetAddress = iota
	GidSetBackupMode = iota
	GidSetDropThreshold = iota
	GidSetExpression = iota
	GidSetIgnoreThreshold = iota
	GidSetLogLevel = iota
	GidSetName = iota
	GidSetPassword = iota
	GidSetPort = iota
	GidSetTimezone = iota
	GidShardColumns = iota
	GidShowStmt = iota
	GidStrOperator = iota
	GidString = iota
	GidSuffixExpr = iota
	GidTimeExpr = iota
	GidTimeitStmt = iota
	GidUserColumns = iota
	GidUuid = iota
	GidWhereGroup = iota
	GidWherePool = iota
	GidWhereSeries = iota
	GidWhereServer = iota
	GidWhereShard = iota
	GidWhereUser = iota
)

// SiriGrammar returns a compiled goleri grammar.
func SiriGrammar() *goleri.Grammar {
	rFloat := goleri.NewRegex(GidRFloat, regexp.MustCompile(`^[-+]?[0-9]*\.?[0-9]+`))
	rInteger := goleri.NewRegex(GidRInteger, regexp.MustCompile(`^[-+]?[0-9]+`))
	rUinteger := goleri.NewRegex(GidRUinteger, regexp.MustCompile(`^[0-9]+`))
	rTimeStr := goleri.NewRegex(GidRTimeStr, regexp.MustCompile(`^[0-9]+[smhdw]`))
	rSingleqStr := goleri.NewRegex(GidRSingleqStr, regexp.MustCompile(`^(?:'(?:[^']*)')+`))
	rDoubleqStr := goleri.NewRegex(GidRDoubleqStr, regexp.MustCompile(`^(?:"(?:[^"]*)")+`))
	rGraveStr := goleri.NewRegex(GidRGraveStr, regexp.MustCompile(`^(?:` + "`" + `(?:[^` + "`" + `]*)` + "`" + `)+`))
	rUuidStr := goleri.NewRegex(GidRUuidStr, regexp.MustCompile(`^[0-9a-f]{8}\-[0-9a-f]{4}\-[0-9a-f]{4}\-[0-9a-f]{4}\-[0-9a-f]{12}`))
	rRegex := goleri.NewRegex(GidRRegex, regexp.MustCompile(`^(/[^/\\]*(?:\\.[^/\\]*)*/i?)`))
	rComment := goleri.NewRegex(GidRComment, regexp.MustCompile(`^#.*`))
	kAccess := goleri.NewKeyword(GidKAccess, "access", false)
	kActiveHandles := goleri.NewKeyword(GidKActiveHandles, "active_handles", false)
	kAddress := goleri.NewKeyword(GidKAddress, "address", false)
	kAfter := goleri.NewKeyword(GidKAfter, "after", false)
	kAlter := goleri.NewKeyword(GidKAlter, "alter", false)
	kAnd := goleri.NewKeyword(GidKAnd, "and", false)
	kAs := goleri.NewKeyword(GidKAs, "as", false)
	kBackupMode := goleri.NewKeyword(GidKBackupMode, "backup_mode", false)
	kBefore := goleri.NewKeyword(GidKBefore, "before", false)
	kBufferSize := goleri.NewKeyword(GidKBufferSize, "buffer_size", false)
	kBufferPath := goleri.NewKeyword(GidKBufferPath, "buffer_path", false)
	kBetween := goleri.NewKeyword(GidKBetween, "between", false)
	kCount := goleri.NewKeyword(GidKCount, "count", false)
	kCreate := goleri.NewKeyword(GidKCreate, "create", false)
	kCritical := goleri.NewKeyword(GidKCritical, "critical", false)
	kDatabase := goleri.NewKeyword(GidKDatabase, "database", false)
	kDbname := goleri.NewKeyword(GidKDbname, "dbname", false)
	kDbpath := goleri.NewKeyword(GidKDbpath, "dbpath", false)
	kDebug := goleri.NewKeyword(GidKDebug, "debug", false)
	kDerivative := goleri.NewKeyword(GidKDerivative, "derivative", false)
	kDifference := goleri.NewKeyword(GidKDifference, "difference", false)
	kDrop := goleri.NewKeyword(GidKDrop, "drop", false)
	kDropThreshold := goleri.NewKeyword(GidKDropThreshold, "drop_threshold", false)
	kDurationLog := goleri.NewKeyword(GidKDurationLog, "duration_log", false)
	kDurationNum := goleri.NewKeyword(GidKDurationNum, "duration_num", false)
	kEnd := goleri.NewKeyword(GidKEnd, "end", false)
	kError := goleri.NewKeyword(GidKError, "error", false)
	kExpression := goleri.NewKeyword(GidKExpression, "expression", false)
	kFalse := goleri.NewKeyword(GidKFalse, "false", false)
	kFilter := goleri.NewKeyword(GidKFilter, "filter", false)
	kFloat := goleri.NewKeyword(GidKFloat, "float", false)
	kFor := goleri.NewKeyword(GidKFor, "for", false)
	kFrom := goleri.NewKeyword(GidKFrom, "from", false)
	kFull := goleri.NewKeyword(GidKFull, "full", false)
	kGrant := goleri.NewKeyword(GidKGrant, "grant", false)
	kGroup := goleri.NewKeyword(GidKGroup, "group", false)
	kGroups := goleri.NewKeyword(GidKGroups, "groups", false)
	kHelp := goleri.NewChoice(
		GidKHelp,
		true,
		goleri.NewKeyword(NoGid, "help", false),
		goleri.NewToken(NoGid, "?"),
	)
	kInfo := goleri.NewKeyword(GidKInfo, "info", false)
	kIgnoreThreshold := goleri.NewKeyword(GidKIgnoreThreshold, "ignore_threshold", false)
	kInsert := goleri.NewKeyword(GidKInsert, "insert", false)
	kInteger := goleri.NewKeyword(GidKInteger, "integer", false)
	kIntersection := goleri.NewChoice(
		GidKIntersection,
		false,
		goleri.NewToken(NoGid, "&"),
		goleri.NewKeyword(NoGid, "intersection", false),
	)
	kIpSupport := goleri.NewKeyword(GidKIpSupport, "ip_support", false)
	kLength := goleri.NewKeyword(GidKLength, "length", false)
	kLibuv := goleri.NewKeyword(GidKLibuv, "libuv", false)
	kLimit := goleri.NewKeyword(GidKLimit, "limit", false)
	kList := goleri.NewKeyword(GidKList, "list", false)
	kLog := goleri.NewKeyword(GidKLog, "log", false)
	kLogLevel := goleri.NewKeyword(GidKLogLevel, "log_level", false)
	kMax := goleri.NewKeyword(GidKMax, "max", false)
	kMaxOpenFiles := goleri.NewKeyword(GidKMaxOpenFiles, "max_open_files", false)
	kMean := goleri.NewKeyword(GidKMean, "mean", false)
	kMedian := goleri.NewKeyword(GidKMedian, "median", false)
	kMedianLow := goleri.NewKeyword(GidKMedianLow, "median_low", false)
	kMedianHigh := goleri.NewKeyword(GidKMedianHigh, "median_high", false)
	kMemUsage := goleri.NewKeyword(GidKMemUsage, "mem_usage", false)
	kMerge := goleri.NewKeyword(GidKMerge, "merge", false)
	kMin := goleri.NewKeyword(GidKMin, "min", false)
	kModify := goleri.NewKeyword(GidKModify, "modify", false)
	kName := goleri.NewKeyword(GidKName, "name", false)
	kNow := goleri.NewKeyword(GidKNow, "now", false)
	kNumber := goleri.NewKeyword(GidKNumber, "number", false)
	kOnline := goleri.NewKeyword(GidKOnline, "online", false)
	kOpenFiles := goleri.NewKeyword(GidKOpenFiles, "open_files", false)
	kOr := goleri.NewKeyword(GidKOr, "or", false)
	kPassword := goleri.NewKeyword(GidKPassword, "password", false)
	kPoints := goleri.NewKeyword(GidKPoints, "points", false)
	kPool := goleri.NewKeyword(GidKPool, "pool", false)
	kPools := goleri.NewKeyword(GidKPools, "pools", false)
	kPort := goleri.NewKeyword(GidKPort, "port", false)
	kPrefix := goleri.NewKeyword(GidKPrefix, "prefix", false)
	kPvariance := goleri.NewKeyword(GidKPvariance, "pvariance", false)
	kRead := goleri.NewKeyword(GidKRead, "read", false)
	kReceivedPoints := goleri.NewKeyword(GidKReceivedPoints, "received_points", false)
	kReindexProgress := goleri.NewKeyword(GidKReindexProgress, "reindex_progress", false)
	kRevoke := goleri.NewKeyword(GidKRevoke, "revoke", false)
	kSelect := goleri.NewKeyword(GidKSelect, "select", false)
	kSeries := goleri.NewKeyword(GidKSeries, "series", false)
	kServer := goleri.NewKeyword(GidKServer, "server", false)
	kServers := goleri.NewKeyword(GidKServers, "servers", false)
	kSet := goleri.NewKeyword(GidKSet, "set", false)
	kSid := goleri.NewKeyword(GidKSid, "sid", false)
	kShards := goleri.NewKeyword(GidKShards, "shards", false)
	kShow := goleri.NewKeyword(GidKShow, "show", false)
	kSize := goleri.NewKeyword(GidKSize, "size", false)
	kStart := goleri.NewKeyword(GidKStart, "start", false)
	kStartupTime := goleri.NewKeyword(GidKStartupTime, "startup_time", false)
	kStatus := goleri.NewKeyword(GidKStatus, "status", false)
	kString := goleri.NewKeyword(GidKString, "string", false)
	kSuffix := goleri.NewKeyword(GidKSuffix, "suffix", false)
	kSum := goleri.NewKeyword(GidKSum, "sum", false)
	kSymmetricDifference := goleri.NewChoice(
		GidKSymmetricDifference,
		false,
		goleri.NewToken(NoGid, "^"),
		goleri.NewKeyword(NoGid, "symmetric_difference", false),
	)
	kSyncProgress := goleri.NewKeyword(GidKSyncProgress, "sync_progress", false)
	kTimeit := goleri.NewKeyword(GidKTimeit, "timeit", false)
	kTimezone := goleri.NewKeyword(GidKTimezone, "timezone", false)
	kTimePrecision := goleri.NewKeyword(GidKTimePrecision, "time_precision", false)
	kTo := goleri.NewKeyword(GidKTo, "to", false)
	kTrue := goleri.NewKeyword(GidKTrue, "true", false)
	kType := goleri.NewKeyword(GidKType, "type", false)
	kUnion := goleri.NewChoice(
		GidKUnion,
		false,
		goleri.NewTokens(NoGid, ", |"),
		goleri.NewKeyword(NoGid, "union", false),
	)
	kUptime := goleri.NewKeyword(GidKUptime, "uptime", false)
	kUser := goleri.NewKeyword(GidKUser, "user", false)
	kUsers := goleri.NewKeyword(GidKUsers, "users", false)
	kUsing := goleri.NewKeyword(GidKUsing, "using", false)
	kUuid := goleri.NewKeyword(GidKUuid, "uuid", false)
	kVariance := goleri.NewKeyword(GidKVariance, "variance", false)
	kVersion := goleri.NewKeyword(GidKVersion, "version", false)
	kWarning := goleri.NewKeyword(GidKWarning, "warning", false)
	kWhere := goleri.NewKeyword(GidKWhere, "where", false)
	kWhoAmI := goleri.NewKeyword(GidKWhoAmI, "who_am_i", false)
	kWrite := goleri.NewKeyword(GidKWrite, "write", false)
	cDifference := goleri.NewChoice(
		GidCDifference,
		false,
		goleri.NewToken(NoGid, "-"),
		kDifference,
	)
	accessKeywords := goleri.NewChoice(
		GidAccessKeywords,
		false,
		kRead,
		kWrite,
		kModify,
		kFull,
		kSelect,
		kShow,
		kList,
		kCount,
		kCreate,
		kInsert,
		kDrop,
		kGrant,
		kRevoke,
		kAlter,
	)
	Boolean := goleri.NewChoice(
		GidBoolean,
		false,
		kTrue,
		kFalse,
	)
	logKeywords := goleri.NewChoice(
		GidLogKeywords,
		false,
		kDebug,
		kInfo,
		kWarning,
		kError,
		kCritical,
	)
	intExpr := goleri.NewPrio(
		GidIntExpr,
		rInteger,
		goleri.NewSequence(
			NoGid,
			goleri.NewToken(NoGid, "("),
			goleri.THIS,
			goleri.NewToken(NoGid, ")"),
		),
		goleri.NewSequence(
			NoGid,
			goleri.THIS,
			goleri.NewTokens(NoGid, "+ - * % /"),
			goleri.THIS,
		),
	)
	string := goleri.NewChoice(
		GidString,
		false,
		rSingleqStr,
		rDoubleqStr,
	)
	timeExpr := goleri.NewPrio(
		GidTimeExpr,
		rTimeStr,
		kNow,
		string,
		rInteger,
		goleri.NewSequence(
			NoGid,
			goleri.NewToken(NoGid, "("),
			goleri.THIS,
			goleri.NewToken(NoGid, ")"),
		),
		goleri.NewSequence(
			NoGid,
			goleri.THIS,
			goleri.NewTokens(NoGid, "+ - * % /"),
			goleri.THIS,
		),
	)
	seriesColumns := goleri.NewList(GidSeriesColumns, goleri.NewChoice(
		NoGid,
		false,
		kName,
		kType,
		kLength,
		kStart,
		kEnd,
		kPool,
	), goleri.NewToken(NoGid, ","), 1, 0, false)
	shardColumns := goleri.NewList(GidShardColumns, goleri.NewChoice(
		NoGid,
		false,
		kSid,
		kPool,
		kServer,
		kSize,
		kStart,
		kEnd,
		kType,
		kStatus,
	), goleri.NewToken(NoGid, ","), 1, 0, false)
	serverColumns := goleri.NewList(GidServerColumns, goleri.NewChoice(
		NoGid,
		false,
		kAddress,
		kBufferPath,
		kBufferSize,
		kDbpath,
		kIpSupport,
		kLibuv,
		kName,
		kPort,
		kUuid,
		kPool,
		kVersion,
		kOnline,
		kStartupTime,
		kStatus,
		kActiveHandles,
		kLogLevel,
		kMaxOpenFiles,
		kMemUsage,
		kOpenFiles,
		kReceivedPoints,
		kReindexProgress,
		kSyncProgress,
		kUptime,
	), goleri.NewToken(NoGid, ","), 1, 0, false)
	groupColumns := goleri.NewList(GidGroupColumns, goleri.NewChoice(
		NoGid,
		false,
		kExpression,
		kName,
		kSeries,
	), goleri.NewToken(NoGid, ","), 1, 0, false)
	userColumns := goleri.NewList(GidUserColumns, goleri.NewChoice(
		NoGid,
		false,
		kName,
		kAccess,
	), goleri.NewToken(NoGid, ","), 1, 0, false)
	poolProps := goleri.NewChoice(
		GidPoolProps,
		false,
		kPool,
		kServers,
		kSeries,
	)
	poolColumns := goleri.NewList(GidPoolColumns, poolProps, goleri.NewToken(NoGid, ","), 1, 0, false)
	boolOperator := goleri.NewTokens(GidBoolOperator, "== !=")
	intOperator := goleri.NewTokens(GidIntOperator, "== != <= >= < >")
	strOperator := goleri.NewTokens(GidStrOperator, "== != <= >= !~ < > ~")
	whereGroup := goleri.NewSequence(
		GidWhereGroup,
		kWhere,
		goleri.NewPrio(
			NoGid,
			goleri.NewSequence(
				NoGid,
				kSeries,
				intOperator,
				intExpr,
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewChoice(
					NoGid,
					false,
					kExpression,
					kName,
				),
				strOperator,
				string,
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewToken(NoGid, "("),
				goleri.THIS,
				goleri.NewToken(NoGid, ")"),
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kAnd,
				goleri.THIS,
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kOr,
				goleri.THIS,
			),
		),
	)
	wherePool := goleri.NewSequence(
		GidWherePool,
		kWhere,
		goleri.NewPrio(
			NoGid,
			goleri.NewSequence(
				NoGid,
				poolProps,
				intOperator,
				intExpr,
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewToken(NoGid, "("),
				goleri.THIS,
				goleri.NewToken(NoGid, ")"),
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kAnd,
				goleri.THIS,
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kOr,
				goleri.THIS,
			),
		),
	)
	whereSeries := goleri.NewSequence(
		GidWhereSeries,
		kWhere,
		goleri.NewPrio(
			NoGid,
			goleri.NewSequence(
				NoGid,
				goleri.NewChoice(
					NoGid,
					false,
					kLength,
					kPool,
				),
				intOperator,
				intExpr,
			),
			goleri.NewSequence(
				NoGid,
				kName,
				strOperator,
				string,
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewChoice(
					NoGid,
					false,
					kStart,
					kEnd,
				),
				intOperator,
				timeExpr,
			),
			goleri.NewSequence(
				NoGid,
				kType,
				boolOperator,
				goleri.NewChoice(
					NoGid,
					false,
					kString,
					kInteger,
					kFloat,
				),
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewToken(NoGid, "("),
				goleri.THIS,
				goleri.NewToken(NoGid, ")"),
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kAnd,
				goleri.THIS,
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kOr,
				goleri.THIS,
			),
		),
	)
	whereServer := goleri.NewSequence(
		GidWhereServer,
		kWhere,
		goleri.NewPrio(
			NoGid,
			goleri.NewSequence(
				NoGid,
				goleri.NewChoice(
					NoGid,
					false,
					kActiveHandles,
					kBufferSize,
					kPort,
					kPool,
					kStartupTime,
					kMaxOpenFiles,
					kMemUsage,
					kOpenFiles,
					kReceivedPoints,
					kUptime,
				),
				intOperator,
				intExpr,
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewChoice(
					NoGid,
					false,
					kAddress,
					kBufferPath,
					kDbpath,
					kIpSupport,
					kLibuv,
					kName,
					kUuid,
					kVersion,
					kStatus,
					kReindexProgress,
					kSyncProgress,
				),
				strOperator,
				string,
			),
			goleri.NewSequence(
				NoGid,
				kOnline,
				boolOperator,
				Boolean,
			),
			goleri.NewSequence(
				NoGid,
				kLogLevel,
				intOperator,
				logKeywords,
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewToken(NoGid, "("),
				goleri.THIS,
				goleri.NewToken(NoGid, ")"),
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kAnd,
				goleri.THIS,
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kOr,
				goleri.THIS,
			),
		),
	)
	whereShard := goleri.NewSequence(
		GidWhereShard,
		kWhere,
		goleri.NewPrio(
			NoGid,
			goleri.NewSequence(
				NoGid,
				goleri.NewChoice(
					NoGid,
					false,
					kSid,
					kPool,
					kSize,
				),
				intOperator,
				intExpr,
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewChoice(
					NoGid,
					true,
					kServer,
					kStatus,
				),
				strOperator,
				string,
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewChoice(
					NoGid,
					false,
					kStart,
					kEnd,
				),
				intOperator,
				timeExpr,
			),
			goleri.NewSequence(
				NoGid,
				kType,
				boolOperator,
				goleri.NewChoice(
					NoGid,
					false,
					kNumber,
					kLog,
				),
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewToken(NoGid, "("),
				goleri.THIS,
				goleri.NewToken(NoGid, ")"),
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kAnd,
				goleri.THIS,
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kOr,
				goleri.THIS,
			),
		),
	)
	whereUser := goleri.NewSequence(
		GidWhereUser,
		kWhere,
		goleri.NewPrio(
			NoGid,
			goleri.NewSequence(
				NoGid,
				kName,
				strOperator,
				string,
			),
			goleri.NewSequence(
				NoGid,
				kAccess,
				intOperator,
				accessKeywords,
			),
			goleri.NewSequence(
				NoGid,
				goleri.NewToken(NoGid, "("),
				goleri.THIS,
				goleri.NewToken(NoGid, ")"),
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kAnd,
				goleri.THIS,
			),
			goleri.NewSequence(
				NoGid,
				goleri.THIS,
				kOr,
				goleri.THIS,
			),
		),
	)
	seriesSep := goleri.NewChoice(
		GidSeriesSep,
		false,
		kUnion,
		cDifference,
		kIntersection,
		kSymmetricDifference,
	)
	seriesName := goleri.NewRepeat(GidSeriesName, string, 1, 1)
	groupName := goleri.NewRepeat(GidGroupName, rGraveStr, 1, 1)
	seriesRe := goleri.NewRepeat(GidSeriesRe, rRegex, 1, 1)
	uuid := goleri.NewChoice(
		GidUuid,
		false,
		rUuidStr,
		string,
	)
	groupMatch := goleri.NewRepeat(GidGroupMatch, rGraveStr, 1, 1)
	seriesMatch := goleri.NewList(GidSeriesMatch, goleri.NewChoice(
		NoGid,
		false,
		seriesName,
		groupMatch,
		seriesRe,
	), seriesSep, 1, 0, false)
	limitExpr := goleri.NewSequence(
		GidLimitExpr,
		kLimit,
		intExpr,
	)
	beforeExpr := goleri.NewSequence(
		GidBeforeExpr,
		kBefore,
		timeExpr,
	)
	afterExpr := goleri.NewSequence(
		GidAfterExpr,
		kAfter,
		timeExpr,
	)
	betweenExpr := goleri.NewSequence(
		GidBetweenExpr,
		kBetween,
		timeExpr,
		kAnd,
		timeExpr,
	)
	accessExpr := goleri.NewList(GidAccessExpr, accessKeywords, goleri.NewToken(NoGid, ","), 1, 0, false)
	prefixExpr := goleri.NewSequence(
		GidPrefixExpr,
		kPrefix,
		string,
	)
	suffixExpr := goleri.NewSequence(
		GidSuffixExpr,
		kSuffix,
		string,
	)
	fPoints := goleri.NewChoice(
		GidFPoints,
		false,
		goleri.NewToken(NoGid, "*"),
		kPoints,
	)
	fDifference := goleri.NewSequence(
		GidFDifference,
		kDifference,
		goleri.NewToken(NoGid, "("),
		goleri.NewOptional(NoGid, timeExpr),
		goleri.NewToken(NoGid, ")"),
	)
	fDerivative := goleri.NewSequence(
		GidFDerivative,
		kDerivative,
		goleri.NewToken(NoGid, "("),
		goleri.NewList(NoGid, timeExpr, goleri.NewToken(NoGid, ","), 0, 2, false),
		goleri.NewToken(NoGid, ")"),
	)
	fMean := goleri.NewSequence(
		GidFMean,
		kMean,
		goleri.NewToken(NoGid, "("),
		timeExpr,
		goleri.NewToken(NoGid, ")"),
	)
	fMedian := goleri.NewSequence(
		GidFMedian,
		kMedian,
		goleri.NewToken(NoGid, "("),
		timeExpr,
		goleri.NewToken(NoGid, ")"),
	)
	fMedianLow := goleri.NewSequence(
		GidFMedianLow,
		kMedianLow,
		goleri.NewToken(NoGid, "("),
		timeExpr,
		goleri.NewToken(NoGid, ")"),
	)
	fMedianHigh := goleri.NewSequence(
		GidFMedianHigh,
		kMedianHigh,
		goleri.NewToken(NoGid, "("),
		timeExpr,
		goleri.NewToken(NoGid, ")"),
	)
	fSum := goleri.NewSequence(
		GidFSum,
		kSum,
		goleri.NewToken(NoGid, "("),
		timeExpr,
		goleri.NewToken(NoGid, ")"),
	)
	fMin := goleri.NewSequence(
		GidFMin,
		kMin,
		goleri.NewToken(NoGid, "("),
		timeExpr,
		goleri.NewToken(NoGid, ")"),
	)
	fMax := goleri.NewSequence(
		GidFMax,
		kMax,
		goleri.NewToken(NoGid, "("),
		timeExpr,
		goleri.NewToken(NoGid, ")"),
	)
	fCount := goleri.NewSequence(
		GidFCount,
		kCount,
		goleri.NewToken(NoGid, "("),
		timeExpr,
		goleri.NewToken(NoGid, ")"),
	)
	fVariance := goleri.NewSequence(
		GidFVariance,
		kVariance,
		goleri.NewToken(NoGid, "("),
		timeExpr,
		goleri.NewToken(NoGid, ")"),
	)
	fPvariance := goleri.NewSequence(
		GidFPvariance,
		kPvariance,
		goleri.NewToken(NoGid, "("),
		timeExpr,
		goleri.NewToken(NoGid, ")"),
	)
	fFilter := goleri.NewSequence(
		GidFFilter,
		kFilter,
		goleri.NewToken(NoGid, "("),
		goleri.NewOptional(NoGid, strOperator),
		goleri.NewChoice(
			NoGid,
			true,
			string,
			rInteger,
			rFloat,
		),
		goleri.NewToken(NoGid, ")"),
	)
	fLimit := goleri.NewSequence(
		GidFLimit,
		kLimit,
		goleri.NewToken(NoGid, "("),
		intExpr,
		goleri.NewToken(NoGid, ","),
		goleri.NewChoice(
			NoGid,
			false,
			kMean,
			kMedian,
			kMedianHigh,
			kMedianLow,
			kSum,
			kMin,
			kMax,
			kCount,
			kVariance,
			kPvariance,
		),
		goleri.NewToken(NoGid, ")"),
	)
	aggregateFunctions := goleri.NewList(GidAggregateFunctions, goleri.NewChoice(
		NoGid,
		false,
		fPoints,
		fLimit,
		fMean,
		fSum,
		fMedian,
		fMedianLow,
		fMedianHigh,
		fMin,
		fMax,
		fCount,
		fVariance,
		fPvariance,
		fDifference,
		fDerivative,
		fFilter,
	), goleri.NewToken(NoGid, "=>"), 1, 0, false)
	selectAggregate := goleri.NewSequence(
		GidSelectAggregate,
		aggregateFunctions,
		goleri.NewOptional(NoGid, prefixExpr),
		goleri.NewOptional(NoGid, suffixExpr),
	)
	mergeAs := goleri.NewSequence(
		GidMergeAs,
		kMerge,
		kAs,
		string,
		goleri.NewOptional(NoGid, goleri.NewSequence(
			NoGid,
			kUsing,
			aggregateFunctions,
		)),
	)
	setAddress := goleri.NewSequence(
		GidSetAddress,
		kSet,
		kAddress,
		string,
	)
	setBackupMode := goleri.NewSequence(
		GidSetBackupMode,
		kSet,
		kBackupMode,
		Boolean,
	)
	setDropThreshold := goleri.NewSequence(
		GidSetDropThreshold,
		kSet,
		kDropThreshold,
		rFloat,
	)
	setExpression := goleri.NewSequence(
		GidSetExpression,
		kSet,
		kExpression,
		rRegex,
	)
	setIgnoreThreshold := goleri.NewSequence(
		GidSetIgnoreThreshold,
		kSet,
		kIgnoreThreshold,
		Boolean,
	)
	setLogLevel := goleri.NewSequence(
		GidSetLogLevel,
		kSet,
		kLogLevel,
		logKeywords,
	)
	setName := goleri.NewSequence(
		GidSetName,
		kSet,
		kName,
		string,
	)
	setPassword := goleri.NewSequence(
		GidSetPassword,
		kSet,
		kPassword,
		string,
	)
	setPort := goleri.NewSequence(
		GidSetPort,
		kSet,
		kPort,
		rUinteger,
	)
	setTimezone := goleri.NewSequence(
		GidSetTimezone,
		kSet,
		kTimezone,
		string,
	)
	alterDatabase := goleri.NewSequence(
		GidAlterDatabase,
		kDatabase,
		goleri.NewChoice(
			NoGid,
			false,
			setDropThreshold,
			setTimezone,
		),
	)
	alterGroup := goleri.NewSequence(
		GidAlterGroup,
		kGroup,
		groupName,
		goleri.NewChoice(
			NoGid,
			false,
			setExpression,
			setName,
		),
	)
	alterServer := goleri.NewSequence(
		GidAlterServer,
		kServer,
		uuid,
		goleri.NewChoice(
			NoGid,
			false,
			setLogLevel,
			setBackupMode,
			setAddress,
			setPort,
		),
	)
	alterServers := goleri.NewSequence(
		GidAlterServers,
		kServers,
		goleri.NewOptional(NoGid, whereServer),
		setLogLevel,
	)
	alterUser := goleri.NewSequence(
		GidAlterUser,
		kUser,
		string,
		goleri.NewChoice(
			NoGid,
			false,
			setPassword,
			setName,
		),
	)
	countGroups := goleri.NewSequence(
		GidCountGroups,
		kGroups,
		goleri.NewOptional(NoGid, whereGroup),
	)
	countPools := goleri.NewSequence(
		GidCountPools,
		kPools,
		goleri.NewOptional(NoGid, wherePool),
	)
	countSeries := goleri.NewSequence(
		GidCountSeries,
		kSeries,
		goleri.NewOptional(NoGid, seriesMatch),
		goleri.NewOptional(NoGid, whereSeries),
	)
	countServers := goleri.NewSequence(
		GidCountServers,
		kServers,
		goleri.NewOptional(NoGid, whereServer),
	)
	countServersReceived := goleri.NewSequence(
		GidCountServersReceived,
		kServers,
		kReceivedPoints,
		goleri.NewOptional(NoGid, whereServer),
	)
	countShards := goleri.NewSequence(
		GidCountShards,
		kShards,
		goleri.NewOptional(NoGid, whereShard),
	)
	countShardsSize := goleri.NewSequence(
		GidCountShardsSize,
		kShards,
		kSize,
		goleri.NewOptional(NoGid, whereShard),
	)
	countUsers := goleri.NewSequence(
		GidCountUsers,
		kUsers,
		goleri.NewOptional(NoGid, whereUser),
	)
	countSeriesLength := goleri.NewSequence(
		GidCountSeriesLength,
		kSeries,
		kLength,
		goleri.NewOptional(NoGid, seriesMatch),
		goleri.NewOptional(NoGid, whereSeries),
	)
	createGroup := goleri.NewSequence(
		GidCreateGroup,
		kGroup,
		groupName,
		kFor,
		rRegex,
	)
	createUser := goleri.NewSequence(
		GidCreateUser,
		kUser,
		string,
		setPassword,
	)
	dropGroup := goleri.NewSequence(
		GidDropGroup,
		kGroup,
		groupName,
	)
	dropSeries := goleri.NewSequence(
		GidDropSeries,
		kSeries,
		goleri.NewOptional(NoGid, seriesMatch),
		goleri.NewOptional(NoGid, whereSeries),
		goleri.NewOptional(NoGid, setIgnoreThreshold),
	)
	dropShards := goleri.NewSequence(
		GidDropShards,
		kShards,
		goleri.NewOptional(NoGid, whereShard),
		goleri.NewOptional(NoGid, setIgnoreThreshold),
	)
	dropServer := goleri.NewSequence(
		GidDropServer,
		kServer,
		uuid,
	)
	dropUser := goleri.NewSequence(
		GidDropUser,
		kUser,
		string,
	)
	grantUser := goleri.NewSequence(
		GidGrantUser,
		kUser,
		string,
		goleri.NewOptional(NoGid, setPassword),
	)
	listGroups := goleri.NewSequence(
		GidListGroups,
		kGroups,
		goleri.NewOptional(NoGid, groupColumns),
		goleri.NewOptional(NoGid, whereGroup),
	)
	listPools := goleri.NewSequence(
		GidListPools,
		kPools,
		goleri.NewOptional(NoGid, poolColumns),
		goleri.NewOptional(NoGid, wherePool),
	)
	listSeries := goleri.NewSequence(
		GidListSeries,
		kSeries,
		goleri.NewOptional(NoGid, seriesColumns),
		goleri.NewOptional(NoGid, seriesMatch),
		goleri.NewOptional(NoGid, whereSeries),
	)
	listServers := goleri.NewSequence(
		GidListServers,
		kServers,
		goleri.NewOptional(NoGid, serverColumns),
		goleri.NewOptional(NoGid, whereServer),
	)
	listShards := goleri.NewSequence(
		GidListShards,
		kShards,
		goleri.NewOptional(NoGid, shardColumns),
		goleri.NewOptional(NoGid, whereShard),
	)
	listUsers := goleri.NewSequence(
		GidListUsers,
		kUsers,
		goleri.NewOptional(NoGid, userColumns),
		goleri.NewOptional(NoGid, whereUser),
	)
	revokeUser := goleri.NewSequence(
		GidRevokeUser,
		kUser,
		string,
	)
	alterStmt := goleri.NewSequence(
		GidAlterStmt,
		kAlter,
		goleri.NewChoice(
			NoGid,
			false,
			alterUser,
			alterGroup,
			alterServer,
			alterServers,
			alterDatabase,
		),
	)
	calcStmt := goleri.NewRepeat(GidCalcStmt, timeExpr, 1, 1)
	countStmt := goleri.NewSequence(
		GidCountStmt,
		kCount,
		goleri.NewChoice(
			NoGid,
			true,
			countGroups,
			countPools,
			countSeries,
			countServers,
			countServersReceived,
			countShards,
			countShardsSize,
			countUsers,
			countSeriesLength,
		),
	)
	createStmt := goleri.NewSequence(
		GidCreateStmt,
		kCreate,
		goleri.NewChoice(
			NoGid,
			true,
			createGroup,
			createUser,
		),
	)
	dropStmt := goleri.NewSequence(
		GidDropStmt,
		kDrop,
		goleri.NewChoice(
			NoGid,
			false,
			dropGroup,
			dropSeries,
			dropShards,
			dropServer,
			dropUser,
		),
	)
	grantStmt := goleri.NewSequence(
		GidGrantStmt,
		kGrant,
		accessExpr,
		kTo,
		goleri.NewChoice(
			NoGid,
			false,
			grantUser,
		),
	)
	listStmt := goleri.NewSequence(
		GidListStmt,
		kList,
		goleri.NewChoice(
			NoGid,
			false,
			listSeries,
			listUsers,
			listShards,
			listGroups,
			listServers,
			listPools,
		),
		goleri.NewOptional(NoGid, limitExpr),
	)
	revokeStmt := goleri.NewSequence(
		GidRevokeStmt,
		kRevoke,
		accessExpr,
		kFrom,
		goleri.NewChoice(
			NoGid,
			false,
			revokeUser,
		),
	)
	selectStmt := goleri.NewSequence(
		GidSelectStmt,
		kSelect,
		goleri.NewList(NoGid, selectAggregate, goleri.NewToken(NoGid, ","), 1, 0, false),
		kFrom,
		seriesMatch,
		goleri.NewOptional(NoGid, whereSeries),
		goleri.NewOptional(NoGid, goleri.NewChoice(
			NoGid,
			false,
			afterExpr,
			betweenExpr,
			beforeExpr,
		)),
		goleri.NewOptional(NoGid, mergeAs),
	)
	showStmt := goleri.NewSequence(
		GidShowStmt,
		kShow,
		goleri.NewList(NoGid, goleri.NewChoice(
			NoGid,
			false,
			kActiveHandles,
			kBufferPath,
			kBufferSize,
			kDbname,
			kDbpath,
			kDropThreshold,
			kDurationLog,
			kDurationNum,
			kIpSupport,
			kLibuv,
			kLogLevel,
			kMaxOpenFiles,
			kMemUsage,
			kOpenFiles,
			kPool,
			kReceivedPoints,
			kReindexProgress,
			kServer,
			kStartupTime,
			kStatus,
			kSyncProgress,
			kTimePrecision,
			kTimezone,
			kUptime,
			kUuid,
			kVersion,
			kWhoAmI,
		), goleri.NewToken(NoGid, ","), 0, 0, false),
	)
	timeitStmt := goleri.NewRepeat(GidTimeitStmt, kTimeit, 1, 1)
	helpCreateGroup := goleri.NewKeyword(GidHelpCreateGroup, "group", false)
	helpCreateUser := goleri.NewKeyword(GidHelpCreateUser, "user", false)
	helpCreate := goleri.NewSequence(
		GidHelpCreate,
		kCreate,
		goleri.NewOptional(NoGid, goleri.NewChoice(
			NoGid,
			true,
			helpCreateGroup,
			helpCreateUser,
		)),
	)
	helpShow := goleri.NewKeyword(GidHelpShow, "show", false)
	helpSelect := goleri.NewKeyword(GidHelpSelect, "select", false)
	helpDropUser := goleri.NewKeyword(GidHelpDropUser, "user", false)
	helpDropServer := goleri.NewKeyword(GidHelpDropServer, "server", false)
	helpDropGroup := goleri.NewKeyword(GidHelpDropGroup, "group", false)
	helpDropShards := goleri.NewKeyword(GidHelpDropShards, "shards", false)
	helpDropSeries := goleri.NewKeyword(GidHelpDropSeries, "series", false)
	helpDrop := goleri.NewSequence(
		GidHelpDrop,
		kDrop,
		goleri.NewOptional(NoGid, goleri.NewChoice(
			NoGid,
			true,
			helpDropUser,
			helpDropServer,
			helpDropGroup,
			helpDropShards,
			helpDropSeries,
		)),
	)
	helpRevoke := goleri.NewKeyword(GidHelpRevoke, "revoke", false)
	helpFunctions := goleri.NewKeyword(GidHelpFunctions, "functions", false)
	helpCountUsers := goleri.NewKeyword(GidHelpCountUsers, "users", false)
	helpCountShards := goleri.NewKeyword(GidHelpCountShards, "shards", false)
	helpCountSeries := goleri.NewKeyword(GidHelpCountSeries, "series", false)
	helpCountGroups := goleri.NewKeyword(GidHelpCountGroups, "groups", false)
	helpCountServers := goleri.NewKeyword(GidHelpCountServers, "servers", false)
	helpCountPools := goleri.NewKeyword(GidHelpCountPools, "pools", false)
	helpCount := goleri.NewSequence(
		GidHelpCount,
		kCount,
		goleri.NewOptional(NoGid, goleri.NewChoice(
			NoGid,
			true,
			helpCountUsers,
			helpCountShards,
			helpCountSeries,
			helpCountGroups,
			helpCountServers,
			helpCountPools,
		)),
	)
	helpAlterServers := goleri.NewKeyword(GidHelpAlterServers, "servers", false)
	helpAlterDatabase := goleri.NewKeyword(GidHelpAlterDatabase, "database", false)
	helpAlterServer := goleri.NewKeyword(GidHelpAlterServer, "server", false)
	helpAlterUser := goleri.NewKeyword(GidHelpAlterUser, "user", false)
	helpAlterGroup := goleri.NewKeyword(GidHelpAlterGroup, "group", false)
	helpAlter := goleri.NewSequence(
		GidHelpAlter,
		kAlter,
		goleri.NewOptional(NoGid, goleri.NewChoice(
			NoGid,
			true,
			helpAlterServers,
			helpAlterDatabase,
			helpAlterServer,
			helpAlterUser,
			helpAlterGroup,
		)),
	)
	helpAccess := goleri.NewKeyword(GidHelpAccess, "access", false)
	helpGrant := goleri.NewKeyword(GidHelpGrant, "grant", false)
	helpTimezones := goleri.NewKeyword(GidHelpTimezones, "timezones", false)
	helpNoaccess := goleri.NewKeyword(GidHelpNoaccess, "noaccess", false)
	helpListShards := goleri.NewKeyword(GidHelpListShards, "shards", false)
	helpListUsers := goleri.NewKeyword(GidHelpListUsers, "users", false)
	helpListPools := goleri.NewKeyword(GidHelpListPools, "pools", false)
	helpListServers := goleri.NewKeyword(GidHelpListServers, "servers", false)
	helpListGroups := goleri.NewKeyword(GidHelpListGroups, "groups", false)
	helpListSeries := goleri.NewKeyword(GidHelpListSeries, "series", false)
	helpList := goleri.NewSequence(
		GidHelpList,
		kList,
		goleri.NewOptional(NoGid, goleri.NewChoice(
			NoGid,
			true,
			helpListShards,
			helpListUsers,
			helpListPools,
			helpListServers,
			helpListGroups,
			helpListSeries,
		)),
	)
	helpTimeit := goleri.NewKeyword(GidHelpTimeit, "timeit", false)
	help := goleri.NewSequence(
		GidHelp,
		kHelp,
		goleri.NewOptional(NoGid, goleri.NewChoice(
			NoGid,
			true,
			helpCreate,
			helpShow,
			helpSelect,
			helpDrop,
			helpRevoke,
			helpFunctions,
			helpCount,
			helpAlter,
			helpAccess,
			helpGrant,
			helpTimezones,
			helpNoaccess,
			helpList,
			helpTimeit,
		)),
	)
	START := goleri.NewSequence(
		GidSTART,
		goleri.NewOptional(NoGid, timeitStmt),
		goleri.NewOptional(NoGid, goleri.NewChoice(
			NoGid,
			false,
			selectStmt,
			listStmt,
			countStmt,
			alterStmt,
			createStmt,
			dropStmt,
			grantStmt,
			revokeStmt,
			showStmt,
			calcStmt,
			help,
		)),
		goleri.NewOptional(NoGid, rComment),
	)
	return goleri.NewGrammar(START, regexp.MustCompile(`^[a-z_]+`))
}
