#include "common/persona.h"

#include "common/database.h"
#include "common/database_schema.h"
#include "common/eqemu_logsys.h"
#include "common/repositories/character_alternate_abilities_repository.h"
#include "common/repositories/character_bandolier_repository.h"
#include "common/repositories/character_bind_repository.h"
#include "common/repositories/character_buffs_repository.h"
#include "common/repositories/character_disciplines_repository.h"
#include "common/repositories/character_equipment_sets_repository.h"
#include "common/repositories/character_memmed_spells_repository.h"
#include "common/repositories/character_personas_repository.h"
#include "common/repositories/character_pet_buffs_repository.h"
#include "common/repositories/character_pet_info_repository.h"
#include "common/repositories/character_pet_inventory_repository.h"
#include "common/repositories/character_skills_repository.h"
#include "common/repositories/character_spells_repository.h"
#include "common/repositories/timers_repository.h"
#include "common/rulesys.h"
#include "common/skills.h"
#include "common/strings.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace {
	// character_data columns that belong to a persona; the class column is handled on its own
	const std::vector<std::string> PERSONA_COLUMNS = {
		"race",
		"level",
		"level2",
		"exp",
		"points",
		"aa_points",
		"aa_exp",
		"aa_points_spent",
		"e_aa_effects",
		"e_percent_to_aa",
		"e_expended_aa_spent",
		"deity",
		"gender",
		"face",
		"hair_color",
		"hair_style",
		"beard",
		"beard_color",
		"eye_color_1",
		"eye_color_2",
		"drakkin_heritage",
		"drakkin_tattoo",
		"drakkin_details",
		"str",
		"sta",
		"cha",
		"dex",
		"int",
		"agi",
		"wis",
		"cur_hp",
		"mana",
		"endurance",
		"ability_number",
		"ability_time_seconds",
		"ability_time_minutes",
		"ability_time_hours",
	};

	constexpr uint32 NEW_PERSONA_LEVEL2 = 1;

	// the mirror of the data_buckets rows that the Character:PersonaDataBuckets rule selects
	const std::string BUCKET_MIRROR_TABLE = "character_persona_data_buckets";

	// escape character for the generated LIKE patterns; chosen so it needs no SQL string escaping
	constexpr char BUCKET_LIKE_ESCAPE = '|';

	std::atomic<bool> s_engines_checked{false};
	std::atomic<bool> s_transactional{false};

	struct MoverTable {
		std::string              live_table;
		std::string              mirror_table;
		std::string              key_column;
		std::vector<std::string> columns; // without the key column, same order on both sides
		std::string              live_filter;
	};

	struct Counts {
		uint32 saved  = 0;
		uint32 loaded = 0;
	};

	std::string Quote(const std::string &name)
	{
		return "`" + name + "`";
	}

	std::string QuoteList(const std::vector<std::string> &names)
	{
		std::string out;
		for (const auto &name : names) {
			if (!out.empty()) {
				out += ", ";
			}

			out += Quote(name);
		}

		return out;
	}

	std::string MovedTimerFilter()
	{
		std::vector<std::string> ranges;
		for (const auto &range : Persona::MOVED_TIMER_RANGES) {
			ranges.push_back(fmt::format("`type` BETWEEN {} AND {}", range.first, range.second));
		}

		return "(" + Strings::Join(ranges, " OR ") + ")";
	}

	std::string SkillFilter()
	{
		std::vector<std::string> shared;
		for (int skill_id = 0; skill_id < EQ::skills::SkillCount; skill_id++) {
			if (Persona::IsSharedSkill(skill_id)) {
				shared.push_back(std::to_string(skill_id));
			}
		}

		return "`skill_id` NOT IN (" + Strings::Join(shared, ",") + ")";
	}

	MoverTable MakeTable(
		const std::string &live_table,
		const std::string &mirror_table,
		const std::vector<std::string> &repository_columns,
		const std::string &live_filter = ""
	)
	{
		MoverTable table;
		table.live_table   = live_table;
		table.mirror_table = mirror_table;
		table.key_column   = DatabaseSchema::GetCharacterTables().at(live_table);
		table.live_filter  = live_filter;

		for (auto column : repository_columns) {
			// the repositories already backtick reserved words
			column.erase(std::remove(column.begin(), column.end(), '`'), column.end());
			if (column != table.key_column) {
				table.columns.push_back(column);
			}
		}

		return table;
	}

	const std::vector<MoverTable> &MoverTables()
	{
		static const std::vector<MoverTable> tables = {
			MakeTable("character_spells", "character_persona_spells", CharacterSpellsRepository::Columns()),
			MakeTable("character_memmed_spells", "character_persona_memmed_spells", CharacterMemmedSpellsRepository::Columns()),
			MakeTable("character_disciplines", "character_persona_disciplines", CharacterDisciplinesRepository::Columns()),
			MakeTable("character_skills", "character_persona_skills", CharacterSkillsRepository::Columns(), SkillFilter()),
			MakeTable("character_alternate_abilities", "character_persona_alternate_abilities", CharacterAlternateAbilitiesRepository::Columns()),
			MakeTable("character_bind", "character_persona_bind", CharacterBindRepository::Columns()),
			MakeTable("character_buffs", "character_persona_buffs", CharacterBuffsRepository::Columns()),
			MakeTable("character_bandolier", "character_persona_bandolier", CharacterBandolierRepository::Columns()),
			MakeTable("timers", "character_persona_timers", TimersRepository::Columns(), MovedTimerFilter()),
			MakeTable("character_pet_info", "character_persona_pet_info", CharacterPetInfoRepository::Columns()),
			MakeTable("character_pet_buffs", "character_persona_pet_buffs", CharacterPetBuffsRepository::Columns()),
			MakeTable("character_pet_inventory", "character_persona_pet_inventory", CharacterPetInventoryRepository::Columns()),
		};

		return tables;
	}

	std::string LiveWhere(const MoverTable &table, uint32 character_id)
	{
		std::string where = fmt::format("{} = {}", Quote(table.key_column), character_id);
		if (!table.live_filter.empty()) {
			where += " AND " + table.live_filter;
		}

		return where;
	}

	std::string MirrorWhere(uint32 character_id, uint8 class_id)
	{
		return fmt::format("`character_id` = {} AND `class_id` = {}", character_id, class_id);
	}

	// Rule patterns: {id} is the character id, * matches any text. Empty when nothing is configured.
	std::string DataBucketWhere(uint32 character_id)
	{
		std::string rule = RuleS(Character, PersonaDataBuckets);

		std::vector<std::string> likes;
		for (auto pattern : Strings::Split(rule, ',')) {
			Strings::Trim(pattern);
			if (pattern.empty()) {
				continue;
			}

			pattern = Strings::Replace(pattern, "{id}", std::to_string(character_id));

			std::string like;
			for (const char c : pattern) {
				if (c == '*') {
					like += '%';
					continue;
				}

				if (c == '%' || c == '_' || c == BUCKET_LIKE_ESCAPE) {
					like += BUCKET_LIKE_ESCAPE;
				}

				like += c;
			}

			likes.push_back(
				fmt::format("`key` LIKE '{}' ESCAPE '{}'", Strings::Escape(like), BUCKET_LIKE_ESCAPE)
			);
		}

		if (likes.empty()) {
			return "";
		}

		// buckets that carry the character id in their key are stored without any owner column
		return "`character_id` = 0 AND `account_id` = 0 AND `npc_id` = 0 AND `bot_id` = 0 AND `zone_id` = 0 AND "
			   "`instance_id` = 0 AND (" + Strings::Join(likes, " OR ") + ")";
	}

	// Every statement inside a transaction goes through here: no silent reconnect, failure is logged once
	bool Run(Database &db, const std::string &query, uint32 *rows = nullptr)
	{
		auto results = db.QueryDatabase(query, false);
		if (!results.Success()) {
			LogError("Persona statement failed [{}] [{}]", results.ErrorMessage(), query);
			return false;
		}

		if (rows) {
			*rows += results.RowsAffected();
		}

		return true;
	}

	bool Finish(Database &db, bool ok)
	{
		if (!ok) {
			db.TransactionRollback();
			return false;
		}

		auto commit = db.TransactionCommit();
		if (!commit.Success()) {
			LogError("Persona transaction failed [{}] rolling back", commit.ErrorMessage());
			db.TransactionRollback();
			return false;
		}

		return true;
	}

	// found is false when the character does not exist
	bool ReadActiveClass(Database &db, uint32 character_id, uint8 &active_class, bool &found)
	{
		auto results = db.QueryDatabase(
			fmt::format("SELECT `class` FROM `character_data` WHERE `id` = {} FOR UPDATE", character_id),
			false
		);
		if (!results.Success()) {
			LogError("Persona statement failed [{}]", results.ErrorMessage());
			return false;
		}

		found = false;
		for (auto row : results) {
			active_class = static_cast<uint8>(Strings::ToUnsignedInt(row[0]));
			found        = true;
		}

		return true;
	}

	bool ReadHasPersona(Database &db, uint32 character_id, uint8 class_id, bool &has_persona)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"SELECT `class_id` FROM `character_personas` WHERE {} FOR UPDATE",
				MirrorWhere(character_id, class_id)
			),
			false
		);
		if (!results.Success()) {
			LogError("Persona statement failed [{}]", results.ErrorMessage());
			return false;
		}

		has_persona = results.RowCount() > 0;
		return true;
	}

	bool SnapshotScalars(Database &db, uint32 character_id)
	{
		std::vector<std::string> updates;
		for (const auto &column : PERSONA_COLUMNS) {
			updates.push_back(fmt::format("{0} = VALUES({0})", Quote(column)));
		}

		return Run(
			db,
			fmt::format(
				"INSERT INTO `character_personas` (`character_id`, `class_id`, {0}) "
				"SELECT `id`, `class`, {0} FROM `character_data` WHERE `id` = {1} "
				"ON DUPLICATE KEY UPDATE {2}",
				QuoteList(PERSONA_COLUMNS),
				character_id,
				Strings::Join(updates, ", ")
			)
		);
	}

	bool ActivateScalars(Database &db, uint32 character_id, uint8 class_id)
	{
		std::vector<std::string> sets = {"cd.`class` = p.`class_id`"};
		for (const auto &column : PERSONA_COLUMNS) {
			sets.push_back(fmt::format("cd.{0} = p.{0}", Quote(column)));
		}

		uint32 rows = 0;
		if (!Run(
			db,
			fmt::format(
				"UPDATE `character_data` cd JOIN `character_personas` p "
				"ON p.`character_id` = cd.`id` AND p.`class_id` = {} SET {} WHERE cd.`id` = {}",
				class_id,
				Strings::Join(sets, ", "),
				character_id
			),
			&rows
		)) {
			return false;
		}

		if (rows == 0) {
			LogError("Persona class [{}] of character [{}] could not be activated", class_id, character_id);
			return false;
		}

		return true;
	}

	bool SnapshotBuckets(Database &db, uint32 character_id, uint8 class_id, Counts &counts)
	{
		const std::string where = DataBucketWhere(character_id);
		if (where.empty()) {
			return true;
		}

		return Run(db, fmt::format("DELETE FROM `{}` WHERE {}", BUCKET_MIRROR_TABLE, MirrorWhere(character_id, class_id))) &&
			   Run(
				   db,
				   fmt::format(
					   "INSERT INTO `{}` (`character_id`, `class_id`, `key`, `value`, `expires`) "
					   "SELECT {}, {}, `key`, `value`, COALESCE(`expires`, 0) FROM `data_buckets` WHERE {}",
					   BUCKET_MIRROR_TABLE,
					   character_id,
					   class_id,
					   where
				   ),
				   &counts.saved
			   ) &&
			   Run(db, fmt::format("DELETE FROM `data_buckets` WHERE {}", where));
	}

	bool ActivateBuckets(Database &db, uint32 character_id, uint8 class_id, Counts &counts)
	{
		// mirrors are only ever written for characters that matched the rule at snapshot time
		return Run(
				   db,
				   fmt::format(
					   "INSERT INTO `data_buckets` (`key`, `value`, `expires`, `account_id`, `character_id`, `npc_id`, "
					   "`bot_id`, `zone_id`, `instance_id`) "
					   "SELECT `key`, `value`, `expires`, 0, 0, 0, 0, 0, 0 FROM `{}` WHERE {}",
					   BUCKET_MIRROR_TABLE,
					   MirrorWhere(character_id, class_id)
				   ),
				   &counts.loaded
			   ) &&
			   Run(db, fmt::format("DELETE FROM `{}` WHERE {}", BUCKET_MIRROR_TABLE, MirrorWhere(character_id, class_id)));
	}

	bool SnapshotActive(Database &db, uint32 character_id, uint8 active_class, Counts &counts)
	{
		if (!SnapshotScalars(db, character_id)) {
			return false;
		}

		for (const auto &table : MoverTables()) {
			const std::string columns = QuoteList(table.columns);

			if (!Run(db, fmt::format("DELETE FROM `{}` WHERE {}", table.mirror_table, MirrorWhere(character_id, active_class))) ||
				!Run(
					db,
					fmt::format(
						"INSERT INTO `{}` (`character_id`, `class_id`, {}) SELECT {}, {}, {} FROM `{}` WHERE {}",
						table.mirror_table,
						columns,
						Quote(table.key_column),
						active_class,
						columns,
						table.live_table,
						LiveWhere(table, character_id)
					),
					&counts.saved
				) ||
				!Run(db, fmt::format("DELETE FROM `{}` WHERE {}", table.live_table, LiveWhere(table, character_id)))) {
				return false;
			}
		}

		return SnapshotBuckets(db, character_id, active_class, counts);
	}

	bool ActivateTables(Database &db, uint32 character_id, uint8 class_id, Counts &counts)
	{
		for (const auto &table : MoverTables()) {
			const std::string columns = QuoteList(table.columns);

			if (!Run(
				db,
				fmt::format(
					"INSERT INTO `{}` ({}, {}) SELECT `character_id`, {} FROM `{}` WHERE {}",
					table.live_table,
					Quote(table.key_column),
					columns,
					columns,
					table.mirror_table,
					MirrorWhere(character_id, class_id)
				),
				&counts.loaded
			) ||
				!Run(db, fmt::format("DELETE FROM `{}` WHERE {}", table.mirror_table, MirrorWhere(character_id, class_id)))) {
				return false;
			}
		}

		return ActivateBuckets(db, character_id, class_id, counts);
	}

	bool InsertNewPersona(Database &db, uint32 character_id, const Persona::NewPersona &persona)
	{
		const std::vector<std::pair<std::string, uint32>> values = {
			{"race", persona.race},
			{"level", Persona::NEW_PERSONA_LEVEL},
			{"level2", NEW_PERSONA_LEVEL2},
			{"exp", 0},
			{"points", Persona::NEW_PERSONA_POINTS},
			{"aa_points", 0},
			{"aa_exp", 0},
			{"aa_points_spent", 0},
			{"e_aa_effects", 0},
			{"e_percent_to_aa", 0},
			{"e_expended_aa_spent", 0},
			{"deity", persona.deity},
			{"gender", persona.gender},
			{"face", persona.face},
			{"hair_color", persona.hair_color},
			{"hair_style", persona.hair_style},
			{"beard", persona.beard},
			{"beard_color", persona.beard_color},
			{"eye_color_1", persona.eye_color_1},
			{"eye_color_2", persona.eye_color_2},
			{"drakkin_heritage", persona.drakkin_heritage},
			{"drakkin_tattoo", persona.drakkin_tattoo},
			{"drakkin_details", persona.drakkin_details},
			{"str", persona.str},
			{"sta", persona.sta},
			{"cha", persona.cha},
			{"dex", persona.dex},
			{"int", persona.int_},
			{"agi", persona.agi},
			{"wis", persona.wis},
			{"cur_hp", 0},
			{"mana", 0},
			{"endurance", 0},
			{"ability_number", 0},
			{"ability_time_seconds", 0},
			{"ability_time_minutes", 0},
			{"ability_time_hours", 0},
		};

		std::vector<std::string> columns;
		std::vector<std::string> numbers;
		for (const auto &value : values) {
			columns.push_back(value.first);
			numbers.push_back(std::to_string(value.second));
		}

		return Run(
			db,
			fmt::format(
				"INSERT INTO `character_personas` (`character_id`, `class_id`, {}) VALUES ({}, {}, {})",
				QuoteList(columns),
				character_id,
				persona.class_id,
				Strings::Join(numbers, ", ")
			)
		);
	}

	// the new class owns no mirror rows; its skills and binds go straight into the live tables
	bool InsertNewPersonaRows(Database &db, uint32 character_id, const Persona::NewPersona &persona)
	{
		std::vector<std::string> skills;
		for (const auto &skill : persona.skills) {
			if (Persona::IsSharedSkill(skill.first)) {
				continue;
			}

			skills.push_back(fmt::format("({}, {}, {})", character_id, skill.first, skill.second));
		}

		if (!skills.empty() &&
			!Run(
				db,
				fmt::format(
					"INSERT INTO `character_skills` (`id`, `skill_id`, `value`) VALUES {}",
					Strings::Join(skills, ", ")
				)
			)) {
			return false;
		}

		std::vector<std::string> binds;
		for (const auto &bind : persona.binds) {
			binds.push_back(
				fmt::format(
					"({}, {}, {}, {}, {}, {}, {}, {})",
					character_id,
					bind.slot,
					bind.zone_id,
					bind.instance_id,
					bind.x,
					bind.y,
					bind.z,
					bind.heading
				)
			);
		}

		return binds.empty() ||
			   Run(
				   db,
				   fmt::format(
					   "INSERT INTO `character_bind` (`id`, `slot`, `zone_id`, `instance_id`, `x`, `y`, `z`, `heading`) VALUES {}",
					   Strings::Join(binds, ", ")
				   )
			   );
	}

	// a new persona starts at its own bind point, like a new character; switches keep the position
	bool MoveToNewPersonaBind(Database &db, uint32 character_id, const Persona::NewPersona &persona)
	{
		for (const auto &bind : persona.binds) {
			if (bind.slot != 0) {
				continue;
			}

			return Run(
				db,
				fmt::format(
					"UPDATE `character_data` SET `zone_id` = {}, `zone_instance` = {}, `x` = {}, `y` = {}, `z` = {}, "
					"`heading` = {} WHERE `id` = {}",
					bind.zone_id,
					bind.instance_id,
					bind.x,
					bind.y,
					bind.z,
					bind.heading,
					character_id
				)
			);
		}

		return true;
	}

	bool SwitchInTransaction(Database &db, uint32 character_id, uint8 new_class, Counts &counts)
	{
		uint8 active_class = 0;
		bool  found        = false;
		if (!ReadActiveClass(db, character_id, active_class, found)) {
			return false;
		}

		if (!found || active_class == new_class) {
			LogPersonas("Switch refused for character [{}]: class [{}] is not an alternate", character_id, new_class);
			return false;
		}

		bool has_persona = false;
		if (!ReadHasPersona(db, character_id, new_class, has_persona)) {
			return false;
		}

		if (!has_persona) {
			LogPersonas("Switch refused for character [{}]: class [{}] is not owned", character_id, new_class);
			return false;
		}

		return SnapshotActive(db, character_id, active_class, counts) &&
			   ActivateTables(db, character_id, new_class, counts) &&
			   ActivateScalars(db, character_id, new_class);
	}

	bool CreateInTransaction(Database &db, uint32 character_id, const Persona::NewPersona &persona, Counts &counts)
	{
		uint8 active_class = 0;
		bool  found        = false;
		if (!ReadActiveClass(db, character_id, active_class, found)) {
			return false;
		}

		if (!found || active_class == persona.class_id) {
			LogPersonas("Create refused for character [{}]: class [{}] is the active class", character_id, persona.class_id);
			return false;
		}

		bool has_persona = false;
		if (!ReadHasPersona(db, character_id, persona.class_id, has_persona)) {
			return false;
		}

		if (has_persona) {
			LogPersonas("Create refused for character [{}]: class [{}] is already owned", character_id, persona.class_id);
			return false;
		}

		return SnapshotActive(db, character_id, active_class, counts) &&
			   InsertNewPersona(db, character_id, persona) &&
			   InsertNewPersonaRows(db, character_id, persona) &&
			   ActivateScalars(db, character_id, persona.class_id) &&
			   MoveToNewPersonaBind(db, character_id, persona);
	}

	bool DeleteInTransaction(Database &db, uint32 character_id, uint8 class_id, Counts &counts)
	{
		uint8 active_class = 0;
		bool  found        = false;
		if (!ReadActiveClass(db, character_id, active_class, found)) {
			return false;
		}

		if (!found || active_class == class_id) {
			LogPersonas("Delete refused for character [{}]: class [{}] is the active class", character_id, class_id);
			return false;
		}

		bool has_persona = false;
		if (!ReadHasPersona(db, character_id, class_id, has_persona)) {
			return false;
		}

		if (!has_persona) {
			LogPersonas("Delete refused for character [{}]: class [{}] is not owned", character_id, class_id);
			return false;
		}

		for (const auto &table : MoverTables()) {
			if (!Run(db, fmt::format("DELETE FROM `{}` WHERE {}", table.mirror_table, MirrorWhere(character_id, class_id)), &counts.saved)) {
				return false;
			}
		}

		return Run(db, fmt::format("DELETE FROM `{}` WHERE {}", BUCKET_MIRROR_TABLE, MirrorWhere(character_id, class_id)), &counts.saved) &&
			   Run(db, fmt::format("DELETE FROM `character_equipment_sets` WHERE {}", MirrorWhere(character_id, class_id))) &&
			   Run(db, fmt::format("DELETE FROM `character_personas` WHERE {}", MirrorWhere(character_id, class_id)));
	}

	int64 ElapsedMs(const std::chrono::steady_clock::time_point &start)
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	}
}

bool Persona::IsSharedSkill(uint16 skill_id)
{
	return EQ::skills::IsTradeskill(static_cast<EQ::skills::SkillType>(skill_id));
}

void Persona::CheckStorageEngines(Database &db)
{
	if (s_engines_checked.exchange(true)) {
		return;
	}

	std::vector<std::string> names = {"character_data", "character_personas", "data_buckets", BUCKET_MIRROR_TABLE};
	for (const auto &table : MoverTables()) {
		names.push_back(table.live_table);
		names.push_back(table.mirror_table);
	}

	std::vector<std::string> quoted;
	for (const auto &name : names) {
		quoted.push_back("'" + name + "'");
	}

	auto results = db.QueryDatabase(fmt::format("SHOW TABLE STATUS WHERE `Name` IN ({})", Strings::Join(quoted, ", ")));
	if (!results.Success()) {
		LogError("Persona storage engine check failed [{}]", results.ErrorMessage());
		return;
	}

	size_t found      = 0;
	bool   all_innodb = true;
	for (auto row : results) {
		found++;

		const std::string engine = row[1] ? row[1] : "";
		if (!Strings::EqualFold(engine, "InnoDB")) {
			LogError("Persona table [{}] uses storage engine [{}], personas need InnoDB", row[0], engine);
			all_innodb = false;
		}
	}

	if (found != names.size()) {
		LogError("Persona tables missing: found [{}] of [{}]", found, names.size());
		all_innodb = false;
	}

	s_transactional = all_innodb;
}

bool Persona::IsStorageTransactional()
{
	return s_transactional;
}

Persona::ClassLevels Persona::LoadClassLevels(Database &db, uint32 character_id, uint8 active_class, uint8 active_level)
{
	ClassLevels classes;

	const auto personas = CharacterPersonasRepository::GetWhere(db, fmt::format("`character_id` = {}", character_id));
	for (const auto &persona : personas) {
		classes[persona.class_id] = static_cast<uint8>(persona.level);
	}

	classes[active_class] = active_level;

	return classes;
}

uint32 Persona::GetAlternateCount(const ClassLevels &classes)
{
	return classes.empty() ? 0 : static_cast<uint32>(classes.size() - 1);
}

bool Persona::Switch(Database &db, uint32 character_id, uint8 new_class)
{
	if (!IsStorageTransactional()) {
		LogPersonas("Switch refused for character [{}]: storage is not transactional", character_id);
		return false;
	}

	const auto start = std::chrono::steady_clock::now();

	Counts counts;
	db.TransactionBegin();
	if (!Finish(db, SwitchInTransaction(db, character_id, new_class, counts))) {
		return false;
	}

	LogPersonas(
		"Switch character [{}] to class [{}] saved [{}] loaded [{}] rows in [{}] ms",
		character_id,
		new_class,
		counts.saved,
		counts.loaded,
		ElapsedMs(start)
	);

	return true;
}

bool Persona::Create(Database &db, uint32 character_id, const NewPersona &persona)
{
	if (!IsStorageTransactional()) {
		LogPersonas("Create refused for character [{}]: storage is not transactional", character_id);
		return false;
	}

	const auto start = std::chrono::steady_clock::now();

	Counts counts;
	db.TransactionBegin();
	if (!Finish(db, CreateInTransaction(db, character_id, persona, counts))) {
		return false;
	}

	LogPersonas(
		"Create character [{}] class [{}] saved [{}] rows in [{}] ms",
		character_id,
		persona.class_id,
		counts.saved,
		ElapsedMs(start)
	);

	return true;
}

bool Persona::Delete(Database &db, uint32 character_id, uint8 class_id)
{
	const auto start = std::chrono::steady_clock::now();

	Counts counts;
	db.TransactionBegin();
	if (!Finish(db, DeleteInTransaction(db, character_id, class_id, counts))) {
		return false;
	}

	LogPersonas(
		"Delete character [{}] class [{}] removed [{}] rows in [{}] ms",
		character_id,
		class_id,
		counts.saved,
		ElapsedMs(start)
	);

	return true;
}

void Persona::UpdateLevel(Database &db, uint32 character_id, uint8 class_id, uint8 level)
{
	db.QueryDatabase(
		fmt::format(
			"UPDATE `character_personas` SET `level` = {} WHERE {}",
			level,
			MirrorWhere(character_id, class_id)
		)
	);
}

void Persona::SaveEquipmentSet(Database &db, uint32 character_id, uint8 class_id, const std::vector<EquippedItem> &items)
{
	CharacterEquipmentSetsRepository::DeleteWhere(db, MirrorWhere(character_id, class_id));

	std::vector<CharacterEquipmentSetsRepository::CharacterEquipmentSets> rows;
	rows.reserve(items.size());
	for (const auto &item : items) {
		auto row         = CharacterEquipmentSetsRepository::NewEntity();
		row.character_id = character_id;
		row.class_id     = class_id;
		row.slot_id      = static_cast<uint16_t>(item.slot_id);
		row.item_serial  = item.item_serial;
		rows.push_back(row);
	}

	if (!rows.empty()) {
		CharacterEquipmentSetsRepository::InsertMany(db, rows);
	}
}

std::vector<Persona::EquippedItem> Persona::LoadEquipmentSet(Database &db, uint32 character_id, uint8 class_id)
{
	std::vector<EquippedItem> items;
	for (const auto &row : CharacterEquipmentSetsRepository::GetWhere(db, MirrorWhere(character_id, class_id))) {
		items.push_back({static_cast<int16>(row.slot_id), row.item_serial});
	}

	return items;
}

std::map<uint32, uint16> Persona::LoadEquipmentSetOwners(Database &db, uint32 character_id)
{
	std::map<uint32, uint16> owners;
	for (const auto &row : CharacterEquipmentSetsRepository::GetWhere(db, fmt::format("`character_id` = {}", character_id))) {
		if (row.class_id >= Class::Warrior && row.class_id <= Class::Berserker) {
			owners[row.item_serial] |= static_cast<uint16>(1 << (row.class_id - 1));
		}
	}

	return owners;
}
