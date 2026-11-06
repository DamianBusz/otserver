BEGIN;

-- Run with Canary stopped so a later player save cannot overwrite these changes.
DO $$
DECLARE
	target_count integer;
BEGIN
	SELECT count(*)
	INTO target_count
	FROM players
	WHERE name IN ('Orenis Sylafin', 'Caelis Dawnward');

	IF target_count <> 2 THEN
		RAISE EXCEPTION 'Expected exactly two target characters, found %', target_count;
	END IF;

	IF EXISTS (
		SELECT 1
		FROM players_online po
		JOIN players p ON p.id = po.player_id
		WHERE p.name IN ('Orenis Sylafin', 'Caelis Dawnward')
	) THEN
		RAISE EXCEPTION 'One or more target characters are online';
	END IF;

	IF EXISTS (
		SELECT 1
		FROM players
		WHERE name IN ('Orenis Sylafin', 'Caelis Dawnward')
			AND vocation NOT IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
	) THEN
		RAISE EXCEPTION 'A target character has an unsupported vocation';
	END IF;
END
$$;

UPDATE players
SET level = 2000,
	experience = 132933899800,
	healthmax = CASE
		WHEN vocation IN (1, 2, 5, 6) THEN 10145
		WHEN vocation IN (3, 7) THEN 20105
		WHEN vocation IN (4, 8) THEN 30065
		WHEN vocation IN (9, 10) THEN 20105
	END,
	health = CASE
		WHEN vocation IN (1, 2, 5, 6) THEN 10145
		WHEN vocation IN (3, 7) THEN 20105
		WHEN vocation IN (4, 8) THEN 30065
		WHEN vocation IN (9, 10) THEN 20105
	END,
	manamax = CASE
		WHEN vocation IN (1, 2, 5, 6) THEN 59795
		WHEN vocation IN (3, 7) THEN 29915
		WHEN vocation IN (4, 8) THEN 9995
		WHEN vocation IN (9, 10) THEN 19955
	END,
	mana = CASE
		WHEN vocation IN (1, 2, 5, 6) THEN 59795
		WHEN vocation IN (3, 7) THEN 29915
		WHEN vocation IN (4, 8) THEN 9995
		WHEN vocation IN (9, 10) THEN 19955
	END,
	cap = CASE
		WHEN vocation IN (1, 2, 5, 6) THEN 20390
		WHEN vocation IN (3, 7) THEN 40310
		WHEN vocation IN (4, 8, 9, 10) THEN 50270
	END,
	maglevel = CASE
		WHEN vocation IN (1, 2, 5, 6) THEN GREATEST(maglevel, 130)
		WHEN vocation IN (3, 7) THEN GREATEST(maglevel, 50)
		ELSE maglevel
	END,
	skill_dist = CASE
		WHEN vocation IN (3, 7) THEN GREATEST(skill_dist, 140)
		ELSE skill_dist
	END,
	skill_sword = CASE
		WHEN vocation IN (4, 8) THEN GREATEST(skill_sword, 130)
		ELSE skill_sword
	END,
	skill_axe = CASE
		WHEN vocation IN (4, 8) THEN GREATEST(skill_axe, 130)
		ELSE skill_axe
	END,
	skill_club = CASE
		WHEN vocation IN (4, 8) THEN GREATEST(skill_club, 130)
		ELSE skill_club
	END,
	skill_fist = CASE
		WHEN vocation IN (9, 10) THEN GREATEST(skill_fist, 130)
		ELSE skill_fist
	END,
	skill_shielding = CASE
		WHEN vocation IN (3, 4, 7, 8) THEN GREATEST(skill_shielding, 120)
		ELSE skill_shielding
	END
WHERE name IN ('Orenis Sylafin', 'Caelis Dawnward');

UPDATE player_charms pc
SET charm_points = GREATEST(pc.charm_points, 30000),
	max_charm_points = GREATEST(pc.max_charm_points, 30000),
	minor_charm_echoes = GREATEST(pc.minor_charm_echoes, 30000),
	max_minor_charm_echoes = GREATEST(pc.max_minor_charm_echoes, 30000)
FROM players p
WHERE pc.player_id = p.id
	AND p.name IN ('Orenis Sylafin', 'Caelis Dawnward');

INSERT INTO player_charms (
	player_id,
	charm_points,
	max_charm_points,
	minor_charm_echoes,
	max_minor_charm_echoes
)
SELECT p.id, 30000, 30000, 30000, 30000
FROM players p
WHERE p.name IN ('Orenis Sylafin', 'Caelis Dawnward')
	AND NOT EXISTS (
		SELECT 1
		FROM player_charms pc
		WHERE pc.player_id = p.id
	);

-- Put new equipment in the inbox without replacing currently equipped items.
WITH targets AS (
	SELECT
		id,
		CASE
			WHEN vocation IN (1, 5) THEN 1
			WHEN vocation IN (2, 6) THEN 2
			WHEN vocation IN (3, 7) THEN 3
			WHEN vocation IN (4, 8) THEN 4
			WHEN vocation IN (9, 10) THEN 9
		END AS base_vocation
	FROM players
	WHERE name IN ('Orenis Sylafin', 'Caelis Dawnward')
),
gear(base_vocation, itemtype) AS (
	VALUES
		(1, 39151), (1, 39152), (1, 34095), (1, 34092),
		(1, 39185), (1, 43883), (1, 43884),
		(2, 39153), (2, 39154), (2, 34096), (2, 34093),
		(2, 39188), (2, 43886), (2, 43887),
		(3, 39149), (3, 39150), (3, 34094), (3, 39182),
		(3, 43878), (3, 43880), (3, 43881),
		(4, 39147), (4, 39148), (4, 39179), (4, 43865),
		(4, 43867), (4, 43869), (4, 43876),
		(9, 50239), (9, 50240), (9, 50254), (9, 50159)
),
new_gear AS (
	SELECT
		t.id AS player_id,
		g.itemtype,
		(
			COALESCE((
				SELECT max(existing.sid)
				FROM player_inboxitems existing
				WHERE existing.player_id = t.id
			), 100)
			+ row_number() OVER (PARTITION BY t.id ORDER BY g.itemtype)
		)::integer AS sid
	FROM targets t
	JOIN gear g ON g.base_vocation = t.base_vocation
	WHERE NOT EXISTS (
		SELECT 1
		FROM player_inboxitems existing
		WHERE existing.player_id = t.id
			AND existing.itemtype = g.itemtype
	)
)
INSERT INTO player_inboxitems (
	player_id,
	sid,
	pid,
	itemtype,
	count,
	attributes
)
SELECT player_id, sid, 0, itemtype, 1, decode('', 'hex')
FROM new_gear;

SELECT
	p.id,
	p.name,
	p.vocation,
	p.level,
	p.experience,
	p.health,
	p.healthmax,
	p.mana,
	p.manamax,
	p.cap,
	pc.charm_points,
	pc.minor_charm_echoes
FROM players p
LEFT JOIN player_charms pc ON pc.player_id = p.id
WHERE p.name IN ('Orenis Sylafin', 'Caelis Dawnward')
ORDER BY p.name;

COMMIT;
