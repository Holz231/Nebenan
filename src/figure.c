// SPDX-License-Identifier: MIT

// Figures after the hybrid of Overgrowth, see the roadmap of the game, chapters 26 to 28 and 31 to 33, and
// Source/Objects/riggedobject.cpp, Source/Graphics/skeleton.cpp and Data/Scripts/aschar.as in Overgrowth. A figure
// stands animated: its bones are kinematic bodies that the update moves with velocities toward the next pose of its
// clip, so debris bounces off them and Box3D solves none of its joints. A hard hit, a blast, a heavy body pressing on
// it or ground that falls away turn it into an active ragdoll: the bones become dynamic with the velocities they had,
// the hit acts once, spread over the bones near it, and the springs of the joints, bounded in torque, pull toward a
// pose to fall with. Like the ragdoll strength of Overgrowth the springs are as strong as the figure is fast and slack
// after a hard hit. A fast figure flails, a slow one relaxes, and one that flies at something curls up or reaches ahead
// when that is in front of it. Lying still it slows down, falls asleep, and gets up once it finds ground, room and
// nothing on it: kinematic again, with a clip from its front or its back that starts from the pose it lies in, blended
// in over the first part of the clip, as Overgrowth blends the relative bone transforms from the ragdoll to the
// animation. Everything runs in the update after a step, in the order of the figures, with operations that give the
// same bits everywhere.

#include "figure.h"

#include "world.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <string.h>

// Damping ratio of the joint constraints of a figure. Measured on the ragdoll of ragdoll-1: the knees and elbows then
// overshoot their limits by 0.1 rad at most under the weight of the body, with Box3D's 2 by up to 0.37 rad.
#define NB_JOINT_DAMPING_RATIO 5.0f

// The torque limit of the joint springs is an addition to Box3D, see extern/README.md. Without it the springs of a
// ragdoll hold its pose against anything.
#if defined( B3_HAS_SPRING_TORQUE_LIMITS )
#define NB_SPRING_TORQUE_LIMITS 1
#else
#define NB_SPRING_TORQUE_LIMITS 0
#endif

// The cone with its own limit in each of four directions is an addition to Box3D, see extern/README.md. Without it a ball
// joint with such limits gets a round cone as wide as the widest of them, at most 0.5 pi.
#if defined( B3_HAS_SWING_LIMITS )
#define NB_SWING_LIMITS 1
#else
#define NB_SWING_LIMITS 0
#endif

// The armature of joints is an addition to Box3D, see extern/README.md. Without it the joints have none, and a light bone
// between two stiff springs shakes.
#if defined( B3_HAS_JOINT_ARMATURE )
#define NB_JOINT_ARMATURE 1
#else
#define NB_JOINT_ARMATURE 0
#endif

// Below this strength or activation of the muscles the springs of the joints are off
#define NB_MIN_STRENGTH 0.01f

// The muscles, see nbFigureMotors::muscles and nbUpdateMuscles: their time constants to tense and to relax, in seconds,
// and how much more they hold while their joint turns against them. Measured on 4 464 hits on the chest and the belly of
// the humanoid from eight directions, see "Muskeln" in docs/Optimierungen.md.
#define NB_TENSE_TIME 0.03f
#define NB_RELAX_TIME 0.08f
#define NB_BRAKE_STRENGTH 1.5f

// Closer than this the activation of a muscle takes the strength asked of it, it would only creep on in the last bits
#define NB_ACTIVATION_SNAP 1.0e-4f

// The curve of Hill (1938): a muscle that shortens at a part v of its top speed pulls (1 - v) / (1 + 4 v) of its
// strength, and one stretched at a part x pulls more, x / (x + 0.05) of the way to its braking strength
#define NB_HILL_CURVE 4.0f
#define NB_HILL_BRAKE_RISE 0.05f

// A hit of at least this share of the knockdown impulse on a bone whose joint gives way, see nbBoneDef::hitStiffness,
// softens the joints that give way while the figure catches itself: soft for NB_SOFT_HOLD seconds, after hard hits
// without the hold against the world less, see NB_SOFT_HARD_CUT, then back to their stiffness on a smooth step over
// NB_SOFT_BACK seconds, see nbSoftenJoints
#define NB_SOFT_IMPULSE 0.5f
#define NB_SOFT_HOLD 0.15f
#define NB_SOFT_BACK 0.35f

// Without the hold against the world the joints that gave way to a hard hit stay soft for less time, and the trunk
// carries the figure through its steps sooner: from NB_SOFT_HARD_START times the knockdown impulse on NB_SOFT_HOLD gets
// shorter, from NB_SOFT_HARD_FULL times on by NB_SOFT_HARD_CUT of it. An idea of the branch ragdoll-sol, which cut
// three quarters. Over the 4 x 5 264 hits of the tables at 60 Hz 3 032 instead of 3 043 fell with the figure's own
// strength and 4 868 instead of 4 954 without a hold, at 30 Hz over the chest and belly tables 2 958 instead of 2 993
// and 5 585 instead of 5 672. Cut by three quarters 3 039 and 4 886 fell at 60 Hz, cut whole 3 012 and 4 927 with 27
// instead of 12 falls from 50 to 89 N s without a hold, and shorter from once the knockdown impulse on 4 906 with 33 of
// them. Shorter with the hold against the world too, by three quarters, 1 061 instead of 1 051 fell there. Round 67.
#define NB_SOFT_HARD_START 1.5f
#define NB_SOFT_HARD_FULL 2.0f
#define NB_SOFT_HARD_CUT 0.5f

// The share of their stiffness the four joints of the spine of the humanoid keep right after a hit, see
// nbBoneDef::hitStiffness and "Rumpf gibt nach" in docs/Optimierungen.md
#define NB_SPINE_HIT_STIFFNESS 0.3f

// How fast the muscles of the humanoid turn a joint of its trunk, neck or head, in radians per second, see
// nbBoneDef::maxSpeed. The limbs have no top speed: their muscles act through the spring of the joint, whose torque limit
// bounds its damping and its hold on the pose as well. With the curve of Hill the loose arms fell behind the chest and
// swung faster against it, and the swinging leg reached its foothold later, and the figure fell from 60 N s on its chest
// from the front.
#define NB_TRUNK_SPEED 25.0f

// The friction of the soles of the humanoid, see nbBoneDef::friction: rubber on dry concrete sticks with about 1 and
// slides with about 0.8, and Box3D knows one friction for both. With the 0.6 of the rig on its soles, until round 44, a
// foot that carried the figure alone slid away under it: with the hold through the legs, after 80 N s from the side the
// stance foot slid 28 cm while the other stepped 70 cm out, and the figure sank. With 0.8, 0.9 and 1.0, 14, 13 and 13 of
// the 168 light hits on the upper chest fell instead of 25, and with the hold against the world 365, 362 and 354 of the
// 2 064 hits on chest and belly from 90 to 139 N s instead of 385. Since round 57 1.1, on ground of 0.6 mixed by Box3D
// to 0.81 instead of 0.73: the stance foot holds the figure about up so much better, and the twist back of the upper
// body in steps back can stay larger, see NB_BACK_TWIST. With 1.0 and the same twist of all 4 times 5 264 hits 4 053
// fell with the figure's own strength instead of 3 946, 1 510 with the hold against the world instead of 1 487 and
// 5 467 without a hold instead of 5 389, and one light hit with the figure's own strength and two without a hold.
#define NB_SOLE_FRICTION 1.1f

// The damping of the bones of a ragdoll that lies still, in 1/s, reached after two seconds, like the damping of
// Overgrowth that rises by half per second of rest: Box3D lets it fall asleep sooner, and it lies without shivering
#define NB_REST_DAMPING 4.0f
#define NB_REST_DAMPING_RATE 0.5f

// A ragdoll lies still long enough to get up after this many seconds of rest
#define NB_REST_TO_GET_UP 0.25f

// How far ahead a flying ragdoll looks for what it flies at, in seconds of its speed, from what speed, with a sphere of
// this radius, and how fast its curl and brace follow, per second
#define NB_DANGER_TIME 0.35f
#define NB_DANGER_SPEED 1.5f
#define NB_DANGER_RADIUS 0.3f
#define NB_PROTECT_RATE 6.0f

// Seconds of the get-up clip in which it blends in from the pose the ragdoll lay in. Over 24 knockdowns the bones move
// at most 12.7 cm a step meanwhile, 14.9 cm with 0.6 s and 34 cm with 1 s: the clip moves on while the pose still holds
// much of the old one. Then the seconds in which the stand clip blends in after it, and a clip of the program.
#define NB_GET_UP_BLEND 0.8f
#define NB_STAND_FADE 0.25f
#define NB_CLIP_FADE 0.2f

// A standing figure looks for its ground this far above and below its feet, follows it up and down at most this fast
// in meters per second, and falls if it is gone or moves faster than NB_GROUND_SPEED
#define NB_GROUND_ABOVE 0.4f
#define NB_GROUND_BELOW 0.35f
#define NB_GROUND_FOLLOW 2.0f

// Every fourth update a standing figure checks what presses on it, figure by figure in turn
#define NB_CRUSH_PERIOD 4

// A figure that gets up falls again from debris that hits it with this part of the knockdown impulse
#define NB_ABORT_PART 0.25f

// Room to get up: a capsule of this radius from this high over the ground up to the height of the figure, and one this far
// ahead of it, where the figure kneels
#define NB_ROOM_RADIUS 0.22f
#define NB_ROOM_FLOOR 0.2f
#define NB_ROOM_AHEAD 0.45f

// A ragdoll at rest that stands on its feet wakes up standing, as WakeUp of Overgrowth tells standing from falling,
// flipping and rolling, see the roadmap of the game 26.15: both feet on ground they could stand on, the pelvis at least
// this part of the stand height over it, and the spine from the pelvis to the chest within this many radians of up. A
// hand caught in a crack of a wall holds a ragdoll so, and the pelvis is too high for the ground to get up from. The
// stand clip blends in over this many seconds, and the bones in its first pose need room where the feet stand, their
// shapes this much smaller, so what they touch does not count.
#define NB_WAKE_HEIGHT 0.6f
#define NB_WAKE_TILT 1.0f
#define NB_WAKE_FADE 0.6f
#define NB_WAKE_SKIN 0.01f

// What a bone with a spring carries hangs beside the body when its center of mass at rest lies this part of the stand
// height or more beside the axis up through the root, an arm: a hit does not swing it into the body, and it hangs on as
// a pendulum when the trunk turns, see nbFlinch and nbLagCarried
#define NB_SWING_BESIDE 0.1f

// Room elsewhere, see nbFindRoom, for the humanoid of 1.8 m and scaled with its height: a figure without room looks this
// near and this far back along its spine and to its sides, on ground at most this much higher or lower, and moves its body
// there at this speed, or turns it about its pelvis at this rate, in this time at least. The body that moves is a capsule
// along the spine this far ahead of the pelvis and behind it, of this radius, this high over the ground.
#define NB_SHIFT_NEAR 0.3f
#define NB_SHIFT_FAR 0.6f
#define NB_SHIFT_RISE 0.2f
#define NB_SHIFT_SPEED 0.5f
#define NB_TURN_SPEED 2.0f
#define NB_SHIFT_TIME 0.3f
#define NB_LYING_AHEAD 0.6f
#define NB_LYING_BEHIND 0.8f
#define NB_LYING_RADIUS 0.1f
#define NB_LYING_HEIGHT 0.22f

// The most contacts of a bone a check reads
#define NB_CONTACT_CAPACITY 16

// Balance steps, see nbPush, for the humanoid of 1.8 m and scaled with its height: a push that would move the body less
// than this does not make it step, a foot is this long in the air and lifts this high, it steps when it stands this far
// from its place under the body, and after the last step the clip takes the legs back over this time
#define NB_STEP_MIN 0.06f
#define NB_STEP_TIME 0.3f
#define NB_STEP_LIFT 0.08f
#define NB_STEP_SLACK 0.03f
#define NB_FOOT_FADE 0.2f

// A step needs ground this flat under its foot, the cosine of its slope, and a foot stands while the clip holds its sole
// this close to the ground and lets it go from this high
#define NB_STEP_FLATNESS 0.7f
#define NB_SOLE_GAP 0.03f
#define NB_SOLE_LIFTED 0.1f

// Feet on the ground: a sphere of this radius under the middle of a sole finds the ground, every eighth update in turn
// with the other figures or once the sole moved this far, the ground tilts the sole by this many radians at most, and the
// foot follows it at this rate per second. On ground this close to where the clip puts it the clip keeps the legs, and
// between those updates the figure does not look at its feet.
#define NB_FOOT_RADIUS 0.04f
#define NB_FOOT_PERIOD 8
#define NB_FOOT_RECAST 0.02f
#define NB_FOOT_TILT 0.4f
#define NB_FOOT_FOLLOW 12.0f
#define NB_FOOT_FLAT 0.002f

// A leg stretches to this part of its length at most, so the knee stays a little soft, or as far as the clip stretches
// it, and the root goes down at most this far for the legs to reach their feet. Walking, a steered leg stretches to
// NB_LEG_REACH_WALK of its length, its knee nearly straight, and the root goes up as far, see nbSteerFeet, and as it
// turns or slows down, to NB_LEG_REACH_BENT, see NB_GAIT_BEND_SPEED. From NB_GAIT_DESCEND of its swing on, the root goes
// down before a swinging foot lands, see nbSteerFeet and nbLowerSteered: as far as its leg needs when the foot lands,
// from where it lands to the hip where that is then, and of what its leg needs beyond that to reach the foot where it
// is now, NB_SWING_DROP_SHARE. Until round 116 it went down for all of that: running, a foot late in its swing is
// farther ahead of the hip than when it lands, as the hip catches up 7 cm in an update, so the root dipped before every
// landing farther than the landing needed and came up after it. At 2 520 points of changes of the gait with a fixed
// heading the pelvis jumps more than 10 mm in an update at 175 instead of 263 now and the hands more than 15 mm at 234
// instead of 285, and at 312 points facing where it moves the pelvis at 27 instead of 42, at speeds that had not run
// before the pelvis at 74 instead of 87 of 1 008 points and at 24 instead of 30 of 312. With none of it the foot did
// not reach the ground as it landed and snapped down, shins over 37 mm at 510 instead of 355 of 1 152 points, with only
// what the landing needs at 431.
#define NB_LEG_REACH 0.98f
#define NB_LEG_REACH_WALK 0.995f
#define NB_LEG_REACH_BENT 0.96f
#define NB_GAIT_DESCEND 0.4f
#define NB_SWING_DROP_SHARE 0.7f
#define NB_MAX_DROP 0.3f

// A swinging steered leg stretches this part of its length farther than its reach at most: its foot goes where the leg
// reaches, and the knee stays bent instead of snapping straight and back, see nbPlaceFeet. Walking straight on, no
// swinging leg comes that far.
#define NB_LEG_SWING_SLACK 0.002f

// As the gait starts or ends on stairs, or ends as the capsule leaves the ground, the root goes on from where it was,
// the difference fading with a time of NB_ROOT_CARRY seconds and ending below NB_ROOT_CARRY_END meters for the humanoid
// of 1.8 m, see nbCarryRoot
#define NB_ROOT_CARRY 0.15f
#define NB_ROOT_CARRY_END 0.0005f

// On stairs the ground under the body goes no faster than NB_STAIR_FOLLOW off the slope of the stairs, in meters per second
// for the humanoid of 1.8 m, see nbStairGround
#define NB_STAIR_FOLLOW 0.6f

// The bones of the figure at an index are in the collision group NB_FIGURE_GROUP plus the index, negative without self
// collision: far from the small numbers a game gives the groups of its own shapes
#define NB_FIGURE_GROUP 0x100000

// A steered foot behind the hip lifts once its leg would not reach it rolled over its toes by NB_FOOT_ROLL in radians,
// see nbLowerSteered, and until then it rolls as far as its leg needs, up to NB_FOOT_ROLL_MOST, see nbRollToReach: in
// its last update on the ground its leg still reaches it, and the knee does not jump as it lifts. The root of a steered
// figure comes up again after it went down for its legs as a knee gives way, critically damped at a rate of one over
// NB_ROOT_SETTLE seconds. Where its legs took it down within the update, it comes up from the share NB_ROOT_SETTLE_KEEP
// of the rate it went down with: turning about at a run, a foot that just landed and that the hip leaves takes the root
// down by up to 2.2 cm in an update, and once its leg reaches it another way, a root that came up from rest turned on
// the spot, and the pelvis jumped 28.3 mm in an update, with half the rate 20.3 mm. With the whole rate it jumps
// 17.6 mm, but the root goes on down for longer, 18.2 instead of 15.5 cm below standing in that turn, and breaking into
// a run both feet are in the air for longer. A foot that lifts goes on rolling as fast as it rolled on the ground, up
// to NB_FOOT_ROLL_RATE in radians per second. Rolled less than NB_FOOT_ROCK in radians either way, a foot in the air
// rocks between heel and toes, see nbRollFoot.
#define NB_FOOT_ROCK 0.15f
#define NB_FOOT_ROLL 0.7f
#define NB_FOOT_ROLL_MOST 1.0f
#define NB_ROOT_SETTLE 0.05f
#define NB_ROOT_SETTLE_KEEP 0.5f
#define NB_FOOT_ROLL_RATE 12.0f

// Running, and from the update the gait turns to running on, a steered foot behind the hip rolls over its toes as far
// as its leg needs over the next NB_FOOT_ROLL_AHEAD seconds, coming there evenly: the heel lifts before the leg is
// stretched, instead of at once as the knee stops or as the capsule speeds away from the foot, see nbLowerSteered
#define NB_FOOT_ROLL_AHEAD 0.08f

// A steered foot on the ground that rolled over its toes behind the hip and comes beside it again, as the figure turns
// back over it, rolls back flat with a time of NB_FOOT_UNROLL seconds instead of at once, see nbLowerSteered. A foot
// that trails going forward and pushes over its toes, see NB_GAIT_PUSH, as the legs change to walking backward loses
// that push evenly with the same time, critically damped, until less than NB_FOOT_PUSH_GONE of it is left: without
// its push at once, it rolled down 0.1 rad in the update after it had rolled up 0.07 rad, and its nearly straight
// knee made the shin jump up to 72 mm where the figure changed from walking sideways to walking backward.
#define NB_FOOT_UNROLL 0.06f
#define NB_FOOT_PUSH_GONE 0.01f

// Going backward, a steered foot on the ground ahead of the hip, which goes away from it, rolls over its heel as far as
// its leg needs, up to NB_FOOT_HEEL_MOST in radians and no faster than NB_FOOT_HEEL_RATE in radians per second, and it
// lifts once its leg would not reach it rolled over its heel by NB_FOOT_HEEL_LIFT, running by less and less, see
// nbLowerSteered. A foot on the ground comes back down from its toes no faster than NB_FOOT_HEEL_RATE either: with the
// decay of NB_FOOT_UNROLL alone, a foot rolled over its toes by 0.88 rad came down in four updates where the legs
// turned from going sideways to going backward, and the shin jumped up to 99 mm in an update. With the heel 4 cm behind
// the ankle of the humanoid until round 103, rolled over its heel a foot gave its leg only a few centimeters, and small
// moves of the hip turned it far: as far as its leg needed, up to 0.7 rad at any speed, its toes jumped more than 37 mm
// in an update at 230 instead of 180 of 504 points of the changes of the gait. Walking as running, it rolls over its
// heel as far as its leg needs over the next NB_FOOT_ROLL_AHEAD seconds: rolling only once its leg needed it, and no
// faster than NB_FOOT_HEEL_RATE, it lagged behind, and with the heel 6.5 cm behind the ankle, which gives the leg 1.3
// instead of 0.8 cm at NB_FOOT_HEEL_LIFT, the leg lost the foot in the update before it lifted, the root went down and
// the pelvis stopped rising. A foot rolled over the other edge by more than NB_FOOT_EDGE_FLAT in radians comes flat
// first.
#define NB_FOOT_HEEL_MOST 0.35f
#define NB_FOOT_HEEL_LIFT 0.2f
#define NB_FOOT_HEEL_RATE 6.0f

// The legs going forward, a foot on the ground rolls up over its toes no more than NB_FOOT_ROLL_GAIN faster than in the
// last update, in radians per second squared for the humanoid of 1.8 m, from the roll it was given then, see
// nbLowerSteered. Behind the hip a foot rolls as far as its leg needs, and turning about at a run a foot stands flat
// with its leg stretched as the hip comes to lie ahead of its toes from one update to the next: it rolled 0.30 rad in
// that update, its ankle rose 6 cm, the foot jumped 64.6 mm and its shin 76.6 mm. Now it rolls 0.08, 0.25 and 0.5 rad
// over three updates, and they jump 19.7 and 14.5 mm at most there. Going steadily the roll speeds up by 266 at most,
// running forward at 4.8 m/s, by 264 running sideways at 4.5 m/s and by 96 walking, and nothing changes there. At 624
// points of changes of the gait facing where it moves, at speeds that had not run before, what shins jump beyond 37 mm
// adds up to 443 instead of 634 mm, and the hardest jump of a shin is 56.7 instead of 67.2 mm. For legs going backward
// too, a foot that lands on its toes could not roll at once, and from sideways to backward shins jumped up to 116.6 mm.
// Measured from the roll the gait gives a foot between the updates instead of the one it had, the roll stood still as
// the legs changed from backward to forward, and the root went down for it. With 240 the bound changes steady running
// from 4.5 m/s on.
#define NB_FOOT_ROLL_GAIN 300.0f
#define NB_FOOT_EDGE_FLAT 0.01f

// The body a push carries along: a capsule of this radius from this high over the ground to this high, which fixed
// bodies stop, cast this far beyond where the push carries it, for the toes ahead of it
#define NB_BODY_RADIUS 0.1f
#define NB_BODY_LOW 0.35f
#define NB_BODY_HIGH 1.4f
#define NB_BODY_MARGIN 0.12f

// A figure that follows its clip near another figure, see nbBeginFollowing: the time in which its drives take a bone back
// to the clip, in seconds; the gravity its weight is taken at for the caps of the drives, in meters per second squared;
// the lever of the torque of a drive against its force, in meters, for a humanoid of 1.8 m; how far its pelvis may be
// from the clip before it lets go and catches itself, in meters for a humanoid of 1.8 m and in radians; and how long no
// other figure has to be near and how near the clip its pelvis has to be for its bones to be kinematic again
#define NB_FOLLOW_TIME 0.1f
#define NB_FOLLOW_GRAVITY 9.81f

// A drive pulls with at most the mass of its bone times this acceleration of the clip, in meters per second squared, see
// nbFollowClip: a fist that a punch in 0.12 s swings out and stops takes up to 540, and a clip that jumps does not throw
// the bones
#define NB_FOLLOW_MAX_ACCELERATION 981.0f
#define NB_FOLLOW_LEVER 0.25f
#define NB_FOLLOW_LET_GO 0.2f
#define NB_FOLLOW_LET_GO_TURN 0.5f
#define NB_FOLLOW_HOLD 1.0f
#define NB_FOLLOW_SETTLED 0.03f
#define NB_FOLLOW_SETTLED_TURN 0.1f

// A bone of a figure that follows its clip that stays farther than this from where the clip puts it, in meters for a
// humanoid of 1.8 m, for this many seconds is stuck: a fist that went under the arm of the other figure, which the joints
// press down while the clip lowers it, holds by friction. The figure lets go and catches itself.
#define NB_FOLLOW_STUCK 0.25f
#define NB_FOLLOW_STUCK_TIME 0.5f

// Other figures overpower a figure that follows its clip when they push it over the ground with this part of its weight,
// see nbUpdateAnimated: a person on their feet pushes back with a fraction of their weight, the drives with up to all of
// it and with what the clip moves on top
#define NB_FOLLOW_OVERPOWER 0.5f

//----------------------------------------------------------------------------------------------------------------------
// Definitions

nbBoneDef nbDefaultBoneDef( void )
{
	nbBoneDef def = { 0 };
	def.parent = -1;
	def.restTransform = b3Transform_identity;
	def.shape = nb_capsuleBone;
	def.start = (b3Vec3){ 0.0f, 0.05f, 0.0f };
	def.end = (b3Vec3){ 0.0f, 0.25f, 0.0f };
	def.radius = 0.05f;
	def.halfExtents = (b3Vec3){ 0.05f, 0.05f, 0.05f };
	def.mass = 1.0f;
	def.friction = 0.0f;
	def.joint = nb_ballJoint;
	def.jointFrame = b3Quat_identity;
	def.lowerAngle = -0.5f;
	def.upperAngle = 0.5f;
	def.coneAngle = 0.5f;
	def.strength = 1.0f;
	def.stiffness = 1.0f;
	def.damping = 1.0f;
	def.catchDamping = 1.0f;
	def.hitStiffness = 1.0f;
	def.armature = 0.0f;
	def.maxSpeed = 0.0f;
	def.flinch = 0.0f;
	def.flinchHertz = 0.0f;
	def.flinchDamping = 1.0f;
	return def;
}

nbRigDef nbDefaultRigDef( void )
{
	nbRigDef def = { 0 };
	def.friction = 0.6f;
	def.selfCollision = true;
	def.enableCollisionDamage = true;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
}

nbFigureMotors nbDefaultFigureMotors( void )
{
	nbFigureMotors motors = { 0 };
	motors.stiffness = 800.0f;
	motors.dampingRatio = 1.0f;
	motors.maxTorque = 150.0f;
	motors.muscles = false;
	motors.tenseTime = NB_TENSE_TIME;
	motors.relaxTime = NB_RELAX_TIME;
	motors.brakeStrength = NB_BRAKE_STRENGTH;
	motors.strengthPerSpeed = 0.1f;
	motors.fallStrength = 0.8f;
	motors.stunTime = 0.0f;
	motors.uprightTorque = 125.0f;
	motors.uprightStiffness = 1500.0f;
	motors.uprightDampingRatio = 1.0f;
	motors.uprightAgainstWorld = true;
	motors.followStrength = 1.0f;
	return motors;
}

nbFigureDef nbDefaultFigureDef( void )
{
	nbFigureDef def = { 0 };
	def.position = b3ToPos( b3Vec3_zero );
	def.standHeight = NB_HUMANOID_STAND;
	def.motors = nbDefaultFigureMotors();
	def.knockdownImpulse = 50.0f;
	def.flinchScale = 0.008f;
	def.flinchLimit = 0.5f;
	def.flinchHertz = 2.0f;
	def.stepReach = 0.45f;
	def.stepZone = 0.6f;
	def.crushForce = 600.0f;
	def.pushForce = 40.0f;
	def.restSpeed = 0.4f;
	def.getUpDelay = 1.0f;
	def.getUpSpeed = 1.4f;
	def.canGetUp = true;
	def.steering.maxSlope = 0.75f;
	def.steering.acceleration = 10.0f;
	def.steering.braking = 14.0f;
	def.steering.airControl = 0.2f;
	def.steering.turnRate = 4.0f;
	def.steering.turnAcceleration = 25.0f;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
}

//----------------------------------------------------------------------------------------------------------------------
// Humanoid

// A bone along its local y axis from the pivot to the next pivot, as a capsule that fits between them
static nbBoneDef nbLimb( const char* name, int parent, b3Vec3 pivot, b3Quat rest, float length, float radius, float mass,
						 nbJointType joint, b3Quat jointFrame, float lower, float upper, float cone, float strength )
{
	nbBoneDef def = nbDefaultBoneDef();
	def.name = name;
	def.parent = parent;
	def.restTransform.p = pivot;
	def.restTransform.q = rest;
	float inset = b3MinFloat( radius, 0.5f * length );
	def.start = (b3Vec3){ 0.0f, inset, 0.0f };
	def.end = (b3Vec3){ 0.0f, length - inset, 0.0f };
	def.radius = radius;
	def.mass = mass;
	def.joint = joint;
	def.jointFrame = jointFrame;
	def.lowerAngle = lower;
	def.upperAngle = upper;
	def.coneAngle = cone;
	def.strength = strength;
	return def;
}

// A segment of the trunk: a rounded block around a center, on a ball joint at its pivot with its z axis up the spine,
// its limits set by the caller. Four of them make the spine, a strong joint each, four times as stiff as the others, so
// the trunk holds like one joint of the motors.
static nbBoneDef nbSegment( const char* name, int parent, b3Vec3 pivot, b3Vec3 center, b3Vec3 halfExtents, float rounding,
							float mass )
{
	nbBoneDef def = nbDefaultBoneDef();
	def.name = name;
	def.parent = parent;
	def.restTransform.p = pivot;
	def.restTransform.q = b3Quat_identity;
	def.shape = nb_roundedBoxBone;
	def.center = center;
	def.halfExtents = halfExtents;
	def.radius = rounding;
	def.mass = mass;
	def.joint = nb_ballJoint;
	def.jointFrame = b3MakeQuatFromAxisAngle( b3Vec3_axisX, -0.5f * B3_PI );
	def.strength = 1.6f;
	def.stiffness = (float)NB_SPINE_COUNT;
	def.hitStiffness = NB_SPINE_HIT_STIFFNESS;
	return def;
}

// The limits of a ball joint of the humanoid in degrees, see nbBoneDef::lowerSwingX: the swing about the x axis of the
// joint frame back and forth, about its y axis, and the twist about its z axis
static void nbSetLimits( nbBoneDef* def, float lowerX, float upperX, float lowerY, float upperY, float lowerTwist,
						 float upperTwist )
{
	const float degree = B3_PI / 180.0f;
	def->lowerSwingX = lowerX * degree;
	def->upperSwingX = upperX * degree;
	def->lowerSwingY = lowerY * degree;
	def->upperSwingY = upperY * degree;
	def->lowerAngle = lowerTwist * degree;
	def->upperAngle = upperTwist * degree;
}

// The same joint on the other side of the body: its swing about the y axis and its twist turn the other way round
static void nbMirrorLimits( nbBoneDef* def )
{
	float lowerY = def->lowerSwingY;
	def->lowerSwingY = -def->upperSwingY;
	def->upperSwingY = -lowerY;
	float lowerTwist = def->lowerAngle;
	def->lowerAngle = -def->upperAngle;
	def->upperAngle = -lowerTwist;
}

// The masses below add up to this, for a figure of 1.8 m. Hands and feet are heavier than on a person: a foot of 1 kg
// that has to stop the 75 kg landing on it stretches its ankle joint by centimeters for a few steps, a foot of 2.5 kg by
// less than half of that. The solver likes mass ratios below 30 to 1 between joined bodies.
#define NB_HUMANOID_MASS 74.0f
#define NB_HUMANOID_HEIGHT 1.8f

float nbMakeHumanoidBones( float height, float mass, nbBoneDef* bones, int capacity )
{
	if ( bones == NULL || capacity < nb_humanoidBoneCount || height <= 0.0f || mass <= 0.0f )
	{
		return 0.0f;
	}

	float s = height / NB_HUMANOID_HEIGHT;
	float m = mass / NB_HUMANOID_MASS;

	// Built after a mannequin, measured in its picture from behind: the pelvis origin stands 98 cm above the soles, the
	// head reaches from 1.57 to 1.80 m on a short neck. Since round 113 its widths, its head and its arms are those of men
	// of its height and weight, the means of the 107 men of 178 to 182 cm and 70 to 78 kg in the survey of the US Army of
	// 2012, ANSUR II: the mannequin was 7 cm wider at the hips, 8 cm at the chest and 4.5 cm over the arms, its hip joints
	// lay 26 instead of 18 cm apart, its head was as wide as deep and 20 instead of 23 cm high, and its upper arms were 5
	// cm too long. The shoulders turn at 1.43 m, inside balls just beside the top corners of the chest, and the arms hang
	// from them to 66 cm, past the middle of the thighs, a little out from the hips. A limb that hangs down has its local
	// y along world -y. The foot points forward, along +z.
	//
	// The joints move as far as a person's, after the normal ranges of the neutral zero method, the ankle as under the
	// weight of the body. A hinge turns about the axis across the bone, local x. A ball joint has its z axis along the
	// bone and a limit in each of four directions, see nbBoneDef::lowerSwingX. On a limb that hangs, and on the spine,
	// the neck and the foot, a swing about +x bends it forward, flexion, the toes up, and one about +y moves it toward
	// -x, inward on the left, so the right side takes the limits mirrored. Its twist turns its front toward -x. The
	// shoulder lifts the arm 160 degrees forward and out, 40 back and in, the hip the leg 135 forward, so the figure
	// can kneel, crawl and get up, 20 back, 45 out and 25 in.
	b3Quat down = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, B3_PI );
	b3Quat zToY = b3MakeQuatFromAxisAngle( b3Vec3_axisX, -0.5f * B3_PI );
	b3Quat zToX = b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.5f * B3_PI );
	b3Quat footRest = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.5f * B3_PI );
	b3Quat identity = b3Quat_identity;

	// The trunk in five rounded blocks like the mannequin of the reference pictures, stacked like tires: a round pelvis
	// from 83 cm to 1.05 m, 33 cm wide as the hips of the men measured, belly and ribs as two flatter rolls above it,
	// 29.6 cm as their waist and 29 cm, the lower chest 28 cm as their chest at the height of the nipples, and the upper
	// chest 36 cm, between the shoulders, up to its top at 1.56 m. The chest stands out in front of belly and ribs by 4
	// cm, as on the mannequin seen from the side. It is two parts, the lower one of 5 kg and the upper one of 7 that
	// carries the arms and the neck, together as heavy as the chest of one part before round 37 with its center of mass
	// where it was. Neighbours overlap, the next but one lies more than 10 cm away. Until round 112 the blocks were 40,
	// 32, 33, 36 and 38 cm wide.
	nbBoneDef pelvis = nbDefaultBoneDef();
	pelvis.name = "pelvis";
	pelvis.parent = -1;
	pelvis.shape = nb_roundedBoxBone;
	pelvis.center = (b3Vec3){ 0.0f, -0.04f * s, 0.0f };
	pelvis.halfExtents = (b3Vec3){ 0.165f * s, 0.11f * s, 0.11f * s };
	pelvis.radius = 0.08f * s;
	pelvis.mass = 11.0f * m;
	pelvis.joint = nb_fixedJoint;
	bones[nb_humanoidPelvis] = pelvis;

	bones[nb_humanoidBelly] =
		nbSegment( "belly", nb_humanoidPelvis, (b3Vec3){ 0.0f, 0.07f * s, 0.0f }, (b3Vec3){ 0.0f, 0.06f * s, 0.0f },
				   (b3Vec3){ 0.148f * s, 0.06f * s, 0.10f * s }, 0.055f * s, 5.0f * m );
	bones[nb_humanoidRibs] =
		nbSegment( "ribs", nb_humanoidBelly, (b3Vec3){ 0.0f, 0.12f * s, 0.0f }, (b3Vec3){ 0.0f, 0.06f * s, 0.0f },
				   (b3Vec3){ 0.145f * s, 0.055f * s, 0.105f * s }, 0.05f * s, 5.0f * m );
	bones[nb_humanoidLowerChest] = nbSegment( "lower chest", nb_humanoidRibs, (b3Vec3){ 0.0f, 0.11f * s, 0.0f },
											  (b3Vec3){ 0.0f, 0.065f * s, 0.015f * s },
											  (b3Vec3){ 0.14f * s, 0.06f * s, 0.12f * s }, 0.05f * s, 5.0f * m );
	bones[nb_humanoidChest] = nbSegment( "chest", nb_humanoidLowerChest, (b3Vec3){ 0.0f, 0.12f * s, 0.0f },
										 (b3Vec3){ 0.0f, 0.075f * s, 0.015f * s },
										 (b3Vec3){ 0.18f * s, 0.08f * s, 0.125f * s }, 0.07f * s, 7.0f * m );

	// The spine bends 75 degrees forward, 30 back and 34 to either side and turns 40 either way, as a person bends it
	// most in the lumbar spine and turns it most in the thoracic: belly and ribs bend 50 of the 75 degrees forward and 22
	// of the 30 back, the two parts of the chest turn 25 of the 40
	nbSetLimits( bones + nb_humanoidBelly, -12.0f, 25.0f, -10.0f, 10.0f, -5.0f, 5.0f );
	nbSetLimits( bones + nb_humanoidRibs, -10.0f, 25.0f, -12.0f, 12.0f, -10.0f, 10.0f );
	nbSetLimits( bones + nb_humanoidLowerChest, -5.0f, 15.0f, -6.0f, 6.0f, -10.0f, 10.0f );
	nbSetLimits( bones + nb_humanoidChest, -3.0f, 10.0f, -6.0f, 6.0f, -15.0f, 15.0f );

	// The neck from the top of the chest, 12 cm thick, and the head on it: a rounded box as wide, deep and high as the
	// heads of the men measured, 15.3, 20 and 23 cm, from the chin at 1.57 m to the top at 1.80 m. A capsule is as wide
	// as deep: until round 112 the head was one of 19 cm and 20 cm high. Together they bend 45 degrees forward, back and
	// to either side and turn 70 either way, the head the most of the turn.
	nbBoneDef neck = nbLimb( "neck", nb_humanoidChest, (b3Vec3){ 0.0f, 0.14f * s, 0.0f }, identity, 0.08f * s, 0.06f * s,
							 1.0f * m, nb_ballJoint, zToY, 0.0f, 0.0f, 0.0f, 0.3f );
	neck.start = (b3Vec3){ 0.0f, 0.0f, 0.0f };
	neck.end = (b3Vec3){ 0.0f, 0.07f * s, 0.0f };
	nbBoneDef head = nbLimb( "head", nb_humanoidNeck, (b3Vec3){ 0.0f, 0.08f * s, 0.0f }, identity, 0.23f * s, 0.07f * s, 5.0f * m,
							 nb_ballJoint, zToY, 0.0f, 0.0f, 0.0f, 0.3f );
	head.shape = nb_roundedBoxBone;
	head.center = (b3Vec3){ 0.0f, 0.065f * s, 0.0f };
	head.halfExtents = (b3Vec3){ 0.0765f * s, 0.115f * s, 0.1f * s };
	nbSetLimits( &neck, -25.0f, 25.0f, -25.0f, 25.0f, -30.0f, 30.0f );
	nbSetLimits( &head, -20.0f, 20.0f, -20.0f, 20.0f, -40.0f, 40.0f );

	// The neck weighs a kilogram between the chest and the head. At the stiffness of the motors its two springs turn it
	// faster than half the substeps per second and it shakes the head. At 0.3 they hold the head about a degree and a half
	// off its pose while the figure catches itself, and a stiffer neck lets it catch itself less often. An armature on
	// both joints as large as the rotational inertia of the neck about its long axis, about 1 g m^2, makes the turn of
	// the neck about twice as heavy and calms it between its springs, see nbBoneDef::armature. With it the neck would
	// even hold the head at the full stiffness, on one of the joints alone or at half of it the head would still shake.
	neck.stiffness = 0.3f;
	head.stiffness = 0.3f;
	neck.armature = 0.001f * m * s * s;
	head.armature = 0.001f * m * s * s;

	// While the figure catches itself, a hit throws the chest about, and neck and head swung on between their springs,
	// up to 5.3 rad/s against each other after 60 N s on the upper chest. Damped eight times as much there, they turn up
	// to 2.8 rad/s and swing out less than half as much, and the head still goes along with the hit. The idea came from
	// the branch ragdoll-sol, see the roadmap, F48, round 59.
	neck.catchDamping = 8.0f;
	head.catchDamping = 8.0f;

	// A light hit swings the neck and the head after the roadmap of the game, 26.4 and 26.5, the head the farthest and
	// the fastest and a little past its pose, see nbBoneDef::flinch
	neck.flinch = 0.3f;
	neck.flinchHertz = 2.5f;
	neck.flinchDamping = 0.7f;
	head.flinch = 0.3f;
	head.flinchHertz = 3.5f;
	head.flinchDamping = 0.5f;
	bones[nb_humanoidNeck] = neck;
	bones[nb_humanoidHead] = head;

	for ( int side = 0; side < 2; ++side )
	{
		float x = side == 0 ? 1.0f : -1.0f;
		int upperArm = side == 0 ? nb_humanoidUpperArmLeft : nb_humanoidUpperArmRight;
		int forearm = side == 0 ? nb_humanoidForearmLeft : nb_humanoidForearmRight;
		int hand = side == 0 ? nb_humanoidHandLeft : nb_humanoidHandRight;
		int thigh = side == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight;
		int shin = side == 0 ? nb_humanoidShinLeft : nb_humanoidShinRight;
		int foot = side == 0 ? nb_humanoidFootLeft : nb_humanoidFootRight;

		// The shoulder joints lie 40.5 cm apart, a centimeter inside the tips of the shoulders of the men measured, 41.5
		// cm, and 2 cm beside the chest, and the arms hang 3 degrees out from the body, the fingertips 24 cm from the
		// middle. Upper arm, forearm and hand are 30, 27.5 and 19.5 cm long, from joint to joint as on those men: their
		// elbows lie at 1.13 m and their wrists at 86 cm. They were 35, 26 and 17 cm until round 112, with the shoulders
		// 43 cm apart and 2 cm higher, and 37, 27 and 17 cm until round 43, as in the reference picture.
		b3Quat armRest = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, B3_PI + 0.05f * x );
		bones[upperArm] = nbLimb( side == 0 ? "upper arm left" : "upper arm right", nb_humanoidChest,
								  (b3Vec3){ 0.2025f * s * x, 0.03f * s, 0.0f }, armRest, 0.30f * s, 0.05f * s, 2.0f * m,
								  nb_ballJoint, zToY, 0.0f, 0.0f, 0.0f, 0.6f );
		bones[forearm] = nbLimb( side == 0 ? "forearm left" : "forearm right", upperArm, (b3Vec3){ 0.0f, 0.30f * s, 0.0f },
								 identity, 0.275f * s, 0.04f * s, 1.2f * m, nb_hingeJoint, zToX, -0.087f, 2.53f, 0.0f, 0.4f );
		bones[hand] = nbLimb( side == 0 ? "hand left" : "hand right", forearm, (b3Vec3){ 0.0f, 0.275f * s, 0.0f }, identity,
							  0.195f * s, 0.035f * s, 0.8f * m, nb_ballJoint, zToY, 0.0f, 0.0f, 0.0f, 0.15f );

		// The shoulder lifts the arm 160 degrees forward and out, 40 back and in, and turns it 90 in and 60 out. The elbow
		// bends 145 degrees and stretches 5 past straight. The hand, its palm toward the thigh, bends 60 toward it and
		// 55 back, 25 forward to the thumb and 35 back, and turns 80 either way with the forearm.
		nbSetLimits( bones + upperArm, -40.0f, 160.0f, -160.0f, 40.0f, -60.0f, 90.0f );
		nbSetLimits( bones + hand, -35.0f, 25.0f, -55.0f, 60.0f, -80.0f, 80.0f );

		// The arms swing loose from a light hit and after the trunk, slower than the head, see nbBoneDef::flinch
		bones[upperArm].flinch = 0.6f;
		bones[upperArm].flinchHertz = 2.5f;
		bones[upperArm].flinchDamping = 0.5f;
		bones[forearm].flinch = 0.6f;
		bones[forearm].flinchHertz = 3.0f;
		bones[forearm].flinchDamping = 0.5f;
		bones[hand].flinch = 0.5f;
		bones[hand].flinchHertz = 4.0f;
		bones[hand].flinchDamping = 0.6f;

		// The upper arm turns about its own long axis against the weakest spring of the shoulder, which is set against the
		// upper arm alone, see nbJointSpringHertz, but the turn swings the bent forearm and the hand around with it. With
		// the arm of 81 cm the arm then turned so up to 35 rad/s while it rowed: of the hits a figure caught, the upper arm
		// turned faster than 15 rad/s against the chest in 19 %, with the arm of 68 cm before round 36 in 0.2 %. An
		// armature about as large as the rotational inertia of forearm and hand about that axis, 0.03 kg m^2 in the stand
		// and 0.1 with the elbow bent as it rows, calms it, in 0.2 % of the hits again, see nbBoneDef::armature.
		bones[upperArm].armature = 0.05f * m * s * s;

		// The legs carry the body: their joints see far more than their two bones and take three times the damping. The
		// hip joints lie 18 cm apart, as the centers of the heads of the thigh bones of men do, 9 cm from the middle, so
		// the thighs leave 4 cm between them. Until round 112 they lay 26 cm apart, under the outer corners of the pelvis
		// of the mannequin, and the feet walked 14 cm apart where people walk about 10: now they do.
		bones[thigh] =
			nbLimb( side == 0 ? "thigh left" : "thigh right", nb_humanoidPelvis, (b3Vec3){ 0.09f * s * x, -0.03f * s, 0.0f },
					down, 0.45f * s, 0.07f * s, 7.5f * m, nb_ballJoint, zToY, 0.0f, 0.0f, 0.0f, 2.0f );
		bones[thigh].damping = 3.0f;
		bones[shin] = nbLimb( side == 0 ? "shin left" : "shin right", thigh, (b3Vec3){ 0.0f, 0.45f * s, 0.0f }, identity,
							  0.44f * s, 0.06f * s, 3.5f * m, nb_hingeJoint, zToX, -2.44f, 0.087f, 0.0f, 1.5f );
		bones[shin].damping = 3.0f;

		// The hip turns the leg 40 degrees in and 45 out, the knee bends 140 degrees and stretches 5 past straight
		nbSetLimits( bones + thigh, -20.0f, 135.0f, -45.0f, 25.0f, -45.0f, 40.0f );

		// A box with a flat sole 6 cm under the ankle, 27 cm long and 10 cm wide like the foot of a person of 1.8 m, 15
		// percent of the height: the heel lies 6.5 cm behind the ankle and the toes 20.5 cm ahead of it. Until round 103 it
		// was 26 by 9 cm with the heel 4 cm behind the ankle. Of 214 272 hits no round had seen before, on 24 tables at 60,
		// 40, 30, 25 and 20 Hz with every hold, 33 596 instead of 35 606 throw the figure down now, fewer at every rate, and
		// of the light ones up to 60 N s 184 instead of 251. Over the heel a foot gives its leg more, see NB_FOOT_HEEL_MOST
		nbBoneDef footDef = nbDefaultBoneDef();
		footDef.name = side == 0 ? "foot left" : "foot right";
		footDef.parent = shin;
		footDef.restTransform.p = (b3Vec3){ 0.0f, 0.44f * s, 0.0f };
		footDef.restTransform.q = footRest;
		footDef.shape = nb_boxBone;
		footDef.center = (b3Vec3){ 0.0f, 0.07f * s, -0.03f * s };
		footDef.halfExtents = (b3Vec3){ 0.05f * s, 0.135f * s, 0.03f * s };
		footDef.mass = 2.5f * m;
		footDef.friction = NB_SOLE_FRICTION;
		footDef.joint = nb_ballJoint;
		footDef.jointFrame = zToY;
		footDef.strength = 0.8f;
		footDef.damping = 3.0f;

		// The ankle lowers the toes 50 degrees and lifts them 40, as far as under the weight of the body in a lunge: the
		// 20 to 30 of the neutral zero method are measured without it, and with 25 the figure caught itself from a
		// seventh fewer hits. It turns the toes 15 degrees in and 10 out and tilts the sole 30 in and 20 out.
		nbSetLimits( &footDef, -50.0f, 40.0f, -10.0f, 15.0f, -30.0f, 20.0f );
		bones[foot] = footDef;

		if ( side == 1 )
		{
			nbMirrorLimits( bones + upperArm );
			nbMirrorLimits( bones + hand );
			nbMirrorLimits( bones + thigh );
			nbMirrorLimits( bones + foot );
		}
	}

	// The muscles of the trunk pull the less the faster it turns, those of the limbs do not, see NB_TRUNK_SPEED
	for ( int i = 1; i < nb_humanoidUpperArmLeft; ++i )
	{
		bones[i].maxSpeed = NB_TRUNK_SPEED;
	}

	// Hip 3 cm under the pelvis origin, thigh, shin and the sole 6 cm under the ankle make 98 cm with straight legs, and
	// it stands in the knees, see nbMakeIdle
	return NB_HUMANOID_STAND * s;
}

int nbBonePoints( const nbBoneDef* bone, b3Vec3* points )
{
	b3Vec3 c = bone->center;
	b3Vec3 h = bone->halfExtents;
	if ( bone->shape == nb_boxBone )
	{
		for ( int k = 0; k < 8; ++k )
		{
			points[k] = (b3Vec3){ c.x + ( k & 1 ? h.x : -h.x ), c.y + ( k & 2 ? h.y : -h.y ), c.z + ( k & 4 ? h.z : -h.z ) };
		}
		return 8;
	}
	if ( bone->shape != nb_roundedBoxBone )
	{
		return 0;
	}

	// Every corner rounded by points on its three axes and halfway between them, so each edge has two faces
	float r = b3ClampFloat( bone->radius, 0.0f, 0.95f * b3MinFloat( h.x, b3MinFloat( h.y, h.z ) ) );
	float d = 0.70710678f * r;
	int count = 0;
	for ( int k = 0; k < 8; ++k )
	{
		float sx = k & 1 ? 1.0f : -1.0f;
		float sy = k & 2 ? 1.0f : -1.0f;
		float sz = k & 4 ? 1.0f : -1.0f;
		b3Vec3 corner = { c.x + sx * ( h.x - r ), c.y + sy * ( h.y - r ), c.z + sz * ( h.z - r ) };
		const b3Vec3 offsets[6] = { { sx * r, 0.0f, 0.0f },	  { 0.0f, sy * r, 0.0f },	{ 0.0f, 0.0f, sz * r },
									{ sx * d, sy * d, 0.0f }, { sx * d, 0.0f, sz * d }, { 0.0f, sy * d, sz * d } };
		for ( int j = 0; j < 6; ++j )
		{
			points[count++] = b3Add( corner, offsets[j] );
		}
	}
	return count;
}

b3HullData* nbCreateBoneHull( const nbBoneDef* bone )
{
	b3Vec3 points[NB_BONE_POINTS];
	int count = bone != NULL ? nbBonePoints( bone, points ) : 0;
	return count > 0 ? b3CreateHull( points, count, B3_MAX_HULL_VERTICES ) : NULL;
}

static float nbBoneVolume( const nbBoneDef* def )
{
	if ( def->shape == nb_boxBone )
	{
		return 8.0f * def->halfExtents.x * def->halfExtents.y * def->halfExtents.z;
	}

	float length = b3Length( b3Sub( def->end, def->start ) );
	float r = def->radius;
	return B3_PI * r * r * length + ( 4.0f / 3.0f ) * B3_PI * r * r * r;
}

//----------------------------------------------------------------------------------------------------------------------
// Rotations

b3Quat nbPartOf( b3Quat rotation, float weight )
{
	b3Quat q = b3NormalizeQuat( rotation );
	if ( q.s < 0.0f )
	{
		q = b3NegateQuat( q );
	}
	float angle;
	b3Vec3 axis = b3GetAxisAngle( &angle, q );
	if ( b3LengthSquared( axis ) == 0.0f )
	{
		return b3Quat_identity;
	}
	return b3MakeQuatFromAxisAngle( axis, b3ClampFloat( weight, 0.0f, 1.0f ) * angle );
}

// e to the minus x for x from 0 to a few, from its series at a quarter of x squared twice, the same bits everywhere
float nbDecay( float x )
{
	float a = 0.25f * b3ClampFloat( x, 0.0f, 20.0f );
	float grow = 1.0f + a * ( 1.0f + a * ( 0.5f + a * ( 1.0f / 6.0f + a * ( 1.0f / 24.0f + a * ( 1.0f / 120.0f ) ) ) ) );
	grow = grow * grow;
	grow = grow * grow;
	return 1.0f / grow;
}

// A rotation from a rotation vector, its direction the axis and its length the angle
static b3Quat nbQuatFromVector( b3Vec3 vector )
{
	float angle = b3Length( vector );
	if ( angle < 1.0e-6f )
	{
		return b3Quat_identity;
	}
	return b3MakeQuatFromAxisAngle( b3MulSV( 1.0f / angle, vector ), angle );
}

// A damped spring at rest at zero, a rotation vector and its rate, stepped by its exact solution, so it swings the same at
// any step rate: angular frequency omega and damping ratio zeta up to 1. The decay and the turn over a step from functions
// that give the same bits everywhere.
static void nbStepSpring( b3Vec3* x, b3Vec3* v, float omega, float zeta, float timeStep )
{
	if ( zeta >= 1.0f )
	{
		float decay = nbDecay( omega * timeStep );
		b3Vec3 sum = b3MulAdd( *v, omega, *x );
		*x = b3MulSV( decay, b3MulAdd( *x, timeStep, sum ) );
		*v = b3MulSV( decay, b3MulSub( *v, omega * timeStep, sum ) );
		return;
	}

	// With a = zeta omega and w the damped frequency, x = e^-at (x0 cos wt + (v0 + a x0) / w sin wt) and
	// v = e^-at (v0 cos wt - (a v0 + omega^2 x0) / w sin wt)
	float a = zeta * omega;
	float w = omega * sqrtf( 1.0f - zeta * zeta );
	float decay = nbDecay( a * timeStep );
	b3CosSin cs = b3ComputeCosSin( w * timeStep );
	float s = cs.sine / w;
	b3Vec3 x0 = *x;
	b3Vec3 v0 = *v;
	*x = b3MulSV( decay, b3MulAdd( b3MulSV( cs.cosine, x0 ), s, b3MulAdd( v0, a, x0 ) ) );
	*v = b3MulSV( decay, b3MulSub( b3MulSV( cs.cosine, v0 ), s, b3MulAdd( b3MulSV( a, v0 ), omega * omega, x0 ) ) );
}

// The transform of a bone of a figure at rest in the frame of its root
static b3Transform nbRestTransform( const nbFigure* f, int i )
{
	if ( i <= 0 )
	{
		return b3Transform_identity;
	}
	return b3MulTransforms( nbRestTransform( f, f->bones[i].parent ), f->bones[i].rest );
}

// The swing of the bones of a standing figure stops: a figure that falls, catches itself or stands again
static void nbStopSwings( nbFigure* f )
{
	for ( int i = 0; i < f->boneCount; ++i )
	{
		nbBone* bone = f->bones + i;
		bone->swing = b3Vec3_zero;
		bone->swingRate = b3Vec3_zero;
		bone->turnRate = b3Vec3_zero;
		bone->pivotVelocity = b3Vec3_zero;
		bone->carried = b3Vec3_zero;
		bone->carriedVelocity = b3Vec3_zero;
	}
	f->swinging = false;
}

// The rotation about up by an angle, up being y in the frame of the figure
static b3Quat nbHeadingQuat( float heading )
{
	return b3MakeQuatFromAxisAngle( b3Vec3_axisY, heading );
}

// The rotation of a joint as Box3D sees it: joint frame B against joint frame A. A bone rotated by q against its rest
// pose puts frame B at conj(f) q f against frame A, with f the joint frame in the frame of the bone.
static b3Quat nbJointRotation( const nbBone* bone, b3Quat rotation )
{
	return b3NormalizeQuat( b3MulQuat( b3MulQuat( b3Conjugate( bone->jointFrame ), rotation ), bone->jointFrame ) );
}

// Bring a rotation of a joint as Box3D sees it back into the limits of the joint, as Box3D measures them: the twist about
// the z axis of the joint frame, see b3GetTwistAngle, and the swing of that axis, see b3GetSwingAngle. The limits are soft,
// a spring that pulls beyond one pushes the joint through it. The halves of the angles are compared by their cosines and
// sines, without an arc tangent, from ragdoll-11. Returns false for a rotation within the limits, which stays as it is.
static bool nbLimitJointRotation( const nbBone* bone, b3Quat* relative )
{
	// The rotation as swing times twist. The twist is the direction (s, z) on the half circle at half its angle, beyond a
	// limit when it lies past the direction of half the limit. The cosine of half the swing is the length of (s, z).
	b3Quat q = relative->s < 0.0f ? b3NegateQuat( *relative ) : *relative;
	bool aboveUpper = bone->upperHalf.cosine * q.v.z - bone->upperHalf.sine * q.s > 0.0f;
	bool belowLower = bone->lowerHalf.cosine * q.v.z - bone->lowerHalf.sine * q.s < 0.0f;
	float twistSquared = q.s * q.s + q.v.z * q.v.z;
	bool beyondCone = bone->joint == nb_ballJoint && bone->swingLimits == false &&
					  twistSquared < bone->coneHalf.cosine * bone->coneHalf.cosine;

	// A limit in each of four directions measures the swing as Box3D does, by the tangent of a quarter of its angle along
	// its axis, see b3GetSwingLimitSeparation, and lies beyond it when the scale onto the quarter of the ellipse passes 1
	float tangentX = 0.0f;
	float tangentY = 0.0f;
	float scale = 0.0f;
	// A swing shorter than the least of the limits lies inside: its sine of half the angle is the length of (x, y)
	if ( bone->swingLimits && q.v.x * q.v.x + q.v.y * q.v.y >= bone->swingInside )
	{
		float w = sqrtf( twistSquared );
		float invScalar = 1.0f / ( w * ( 1.0f + w ) + FLT_MIN );
		tangentX = ( q.s * q.v.x - q.v.z * q.v.y ) * invScalar;
		tangentY = ( q.v.z * q.v.x + q.s * q.v.y ) * invScalar;
		float ex = tangentX * ( tangentX >= 0.0f ? bone->swingInverseTans[0] : bone->swingInverseTans[1] );
		float ey = tangentY * ( tangentY >= 0.0f ? bone->swingInverseTans[2] : bone->swingInverseTans[3] );
		scale = sqrtf( ex * ex + ey * ey );
		beyondCone = scale > 1.0f;
	}
	if ( aboveUpper == false && belowLower == false && beyondCone == false )
	{
		return false;
	}

	// A hinge turns about its axis alone, its drive ignores the swing
	float twistLength = sqrtf( twistSquared );
	b3Quat twist = twistLength > 0.0f ? (b3Quat){ { 0.0f, 0.0f, q.v.z / twistLength }, q.s / twistLength } : b3Quat_identity;
	b3Quat swing = b3MulQuat( q, b3Conjugate( twist ) );
	if ( aboveUpper || belowLower )
	{
		b3CosSin limit = aboveUpper ? bone->upperHalf : bone->lowerHalf;
		twist = (b3Quat){ { 0.0f, 0.0f, limit.sine }, limit.cosine };
	}
	float swingLength = sqrtf( swing.v.x * swing.v.x + swing.v.y * swing.v.y );
	if ( beyondCone && bone->swingLimits )
	{
		// Onto the edge along the same axis, and back from the tangent of a quarter to the half angle
		float tx = tangentX / scale;
		float ty = tangentY / scale;
		float t2 = tx * tx + ty * ty;
		float inv = 1.0f / ( 1.0f + t2 );
		swing = (b3Quat){ { 2.0f * tx * inv, 2.0f * ty * inv, 0.0f }, ( 1.0f - t2 ) * inv };
	}
	else if ( beyondCone && swingLength > 0.0f )
	{
		float shrink = bone->coneHalf.sine / swingLength;
		swing = (b3Quat){ { shrink * swing.v.x, shrink * swing.v.y, 0.0f }, bone->coneHalf.cosine };
	}
	*relative = b3NormalizeQuat( b3MulQuat( swing, twist ) );
	return true;
}

float nbSwingLimit( const nbBone* bone, float axisX, float axisY )
{
	if ( bone->swingLimits == false )
	{
		return 2.0f * b3Atan2( bone->coneHalf.sine, bone->coneHalf.cosine );
	}

	// The axis reaches the quarter of the ellipse where the tangent of a quarter of the angle is one over its scale
	float ex = axisX * ( axisX >= 0.0f ? bone->swingInverseTans[0] : bone->swingInverseTans[1] );
	float ey = axisY * ( axisY >= 0.0f ? bone->swingInverseTans[2] : bone->swingInverseTans[3] );
	return 4.0f * b3Atan2( 1.0f, sqrtf( ex * ex + ey * ey ) );
}

// The rotation of a bone against its rest pose within the limits of its joint
static b3Quat nbLimitBoneRotation( const nbBone* bone, b3Quat rotation )
{
	if ( bone->joint == nb_fixedJoint )
	{
		return b3Quat_identity;
	}

	b3Quat relative = nbJointRotation( bone, rotation );
	if ( nbLimitJointRotation( bone, &relative ) == false )
	{
		return rotation;
	}
	return b3NormalizeQuat( b3MulQuat( b3MulQuat( bone->jointFrame, relative ), b3Conjugate( bone->jointFrame ) ) );
}

// Box3D sets a joint spring in cycles per second against the rotational mass of its two bodies about the axis of the
// spring: the same cycles make a weak spring on a light foot and a strong one on a heavy thigh. A stiffness in newton
// meters per radian is the same on every joint, this turns it into the cycles per second of one joint, from ragdoll-1. The
// rotational mass is taken about the softest of the axes, so a ball joint is at least this stiff about every axis. The
// armature of the joint adds to it, Box3D sets the spring against both.
float nbJointSpringHertz( b3BodyId bodyA, b3BodyId bodyB, b3Quat frameB, bool hingeOnly, float stiffness, float armature )
{
	if ( stiffness <= 0.0f )
	{
		return 0.0f;
	}

	b3Matrix3 invI =
		b3AddMM( b3Body_GetWorldInverseRotationalInertia( bodyA ), b3Body_GetWorldInverseRotationalInertia( bodyB ) );
	b3Vec3 axes[3] = { b3RotateVector( frameB, b3Vec3_axisX ), b3RotateVector( frameB, b3Vec3_axisY ),
					   b3RotateVector( frameB, b3Vec3_axisZ ) };
	float invMass = 0.0f;
	for ( int k = hingeOnly ? 2 : 0; k < 3; ++k )
	{
		invMass = b3MaxFloat( invMass, b3Dot( axes[k], b3MulMV( invI, axes[k] ) ) );
	}
	if ( invMass <= 0.0f )
	{
		return 0.0f;
	}
	if ( armature > 0.0f )
	{
		invMass = 1.0f / ( 1.0f / invMass + armature );
	}

	return sqrtf( stiffness * invMass ) / ( 2.0f * B3_PI );
}

//----------------------------------------------------------------------------------------------------------------------
// Clips

static void nbFreeClip( nbClip* clip )
{
	if ( clip->keyframeCount > 0 )
	{
		nbFree( clip->times, sizeof( float ) * (size_t)clip->keyframeCount );
		nbFree( clip->rotations, sizeof( b3Quat ) * (size_t)clip->keyframeCount * (size_t)clip->boneCount );
		nbFree( clip->roots, sizeof( b3Transform ) * (size_t)clip->keyframeCount );
	}
	memset( clip, 0, sizeof( nbClip ) );
}

// Copy a clip of the program for the bones of a figure. A clip that does not fit stays empty.
static void nbCopyClip( nbClip* clip, const nbClipDef* def, int boneCount )
{
	nbFreeClip( clip );
	if ( def == NULL || def->keyframes == NULL || def->keyframeCount <= 0 || def->boneCount != boneCount )
	{
		return;
	}

	int count = def->keyframeCount;
	clip->times = nbAlloc( sizeof( float ) * (size_t)count );
	clip->rotations = nbAlloc( sizeof( b3Quat ) * (size_t)count * (size_t)boneCount );
	clip->roots = nbAlloc( sizeof( b3Transform ) * (size_t)count );
	clip->keyframeCount = count;
	clip->boneCount = boneCount;
	clip->loop = def->loop;
	clip->moveRoot = def->moveRoot;
	for ( int k = 0; k < count; ++k )
	{
		const nbKeyframe* key = def->keyframes + k;
		clip->times[k] = k > 0 ? b3MaxFloat( key->time, clip->times[k - 1] ) : key->time;
		b3Transform root = key->root;
		root.q = b3NormalizeQuat( root.q );
		clip->roots[k] = root;
		for ( int i = 0; i < boneCount; ++i )
		{
			clip->rotations[k * boneCount + i] = key->rotations != NULL ? b3NormalizeQuat( key->rotations[i] ) : b3Quat_identity;
		}
	}
}

static float nbClipDuration( const nbClip* clip )
{
	return clip->keyframeCount > 0 ? clip->times[clip->keyframeCount - 1] : 0.0f;
}

// Which two keyframes a time falls between, and how far along, from 0 to 1. Built into each of its callers: called out
// of line, as GCC does for three callers, sampling the clip of a figure costs more than the call
B3_FORCE_INLINE int nbClipSegment( const float* times, size_t stride, int count, bool loop, float time, float* alpha )
{
#define NB_KEY_TIME( k ) ( *(const float*)( (const char*)times + (size_t)( k ) * stride ) )
	if ( count < 2 )
	{
		*alpha = 0.0f;
		return 0;
	}

	float start = NB_KEY_TIME( 0 );
	float end = NB_KEY_TIME( count - 1 );
	if ( loop && end > start )
	{
		float length = end - start;
		time = start + fmodf( time - start, length );
		if ( time < start )
		{
			time += length;
		}
	}
	time = b3ClampFloat( time, start, end );

	int segment = 0;
	while ( segment + 2 < count && time >= NB_KEY_TIME( segment + 1 ) )
	{
		segment += 1;
	}

	float from = NB_KEY_TIME( segment );
	float span = NB_KEY_TIME( segment + 1 ) - from;
	*alpha = span > 0.0f ? b3ClampFloat( ( time - from ) / span, 0.0f, 1.0f ) : 1.0f;
	return segment;
#undef NB_KEY_TIME
}

// A root between two roots
static b3Transform nbBlendRoots( b3Transform a, b3Transform b, float alpha )
{
	return (b3Transform){ b3Lerp( a.p, b.p, alpha ), b3NLerp( a.q, b.q, alpha ) };
}

// The root of a figure while a clip without root motion plays: upright at the standing height
static b3Transform nbStandRoot( const nbFigure* f )
{
	return (b3Transform){ { 0.0f, f->def.standHeight, 0.0f }, b3Quat_identity };
}

// Sample a clip of a figure into a pose and a root. A clip without keyframes is the rest pose.
static void nbSampleFigureClip( const nbFigure* f, const nbClip* clip, float time, b3Quat* pose, b3Transform* root )
{
	if ( clip->keyframeCount == 0 )
	{
		for ( int i = 0; i < f->boneCount; ++i )
		{
			pose[i] = b3Quat_identity;
		}
		*root = nbStandRoot( f );
		return;
	}

	float alpha;
	int segment = nbClipSegment( clip->times, sizeof( float ), clip->keyframeCount, clip->loop, time, &alpha );
	int next = clip->keyframeCount > 1 ? segment + 1 : segment;
	const b3Quat* from = clip->rotations + segment * clip->boneCount;
	const b3Quat* to = clip->rotations + next * clip->boneCount;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		pose[i] = b3NLerp( from[i], to[i], alpha );
	}
	*root = clip->moveRoot ? nbBlendRoots( clip->roots[segment], clip->roots[next], alpha ) : nbStandRoot( f );
}

// A clip of the gait plays a foot standing for no less and no more of its cycle than this, whatever its marks say, so
// both feet swing and both stand
#define NB_GAIT_CLIP_STANCE_LEAST 0.2f
#define NB_GAIT_CLIP_STANCE_MOST 0.8f

// A time wrapped into [0, length)
static float nbWrapTime( float time, float length )
{
	float wrapped = fmodf( time, length );
	return wrapped < 0.0f ? wrapped + length : wrapped;
}

static void nbFreeGaitClips( nbFigure* f )
{
	for ( int k = 0; k < f->gaitClipCount; ++k )
	{
		nbFreeClip( &f->gaitClips[k].clip );
	}
	memset( f->gaitClips, 0, sizeof( f->gaitClips ) );
	f->gaitClipCount = 0;
	f->gaitClipRoot = false;
}

// Copy the clips of the gait of the program for a humanoid, the slower first, see nbGaitClip. A clip that does not fit
// the bones or has no cycle stays out. It loops, and a foot stands for as long a share of its cycle as both feet stand
// in it on average, see NB_GAIT_CLIP_STANCE_LEAST.
static void nbCopyGaitClips( nbFigure* f, const nbGaitClipDef* defs, int count )
{
	nbFreeGaitClips( f );
	if ( defs == NULL || f->boneCount != nb_humanoidBoneCount )
	{
		return;
	}

	bool moveRoot = true;
	for ( int i = 0; i < b3MinInt( count, NB_MAX_GAIT_CLIPS ); ++i )
	{
		const nbGaitClipDef* def = defs + i;
		nbGaitClip* gait = f->gaitClips + f->gaitClipCount;
		nbCopyClip( &gait->clip, &def->clip, f->boneCount );
		int last = gait->clip.keyframeCount - 1;
		float cycle = last > 0 ? gait->clip.times[last] - gait->clip.times[0] : 0.0f;
		if ( cycle <= 0.0f || ( def->speed >= 0.0f ) == false )
		{
			nbFreeClip( &gait->clip );
			continue;
		}

		gait->clip.loop = true;
		gait->speed = def->speed;
		gait->start = gait->clip.times[0];
		gait->cycle = cycle;
		float stance = 0.0f;
		for ( int k = 0; k < 2; ++k )
		{
			gait->down[k] = nbWrapTime( def->footDown[k], cycle );
			stance += nbWrapTime( def->footUp[k] - def->footDown[k], cycle ) / cycle;
		}
		gait->stance = b3ClampFloat( 0.5f * stance, NB_GAIT_CLIP_STANCE_LEAST, NB_GAIT_CLIP_STANCE_MOST );
		gait->drift = b3Vec3_zero;
		gait->level = f->def.standHeight;
		if ( gait->clip.moveRoot )
		{
			b3Vec3 way = b3Sub( gait->clip.roots[last].p, gait->clip.roots[0].p );
			gait->drift = (b3Vec3){ way.x / cycle, 0.0f, way.z / cycle };
			float area = 0.0f;
			for ( int k = 0; k < last; ++k )
			{
				float span = gait->clip.times[k + 1] - gait->clip.times[k];
				area += 0.5f * span * ( gait->clip.roots[k].p.y + gait->clip.roots[k + 1].p.y );
			}
			gait->level = area / cycle;
		}
		moveRoot = moveRoot && gait->clip.moveRoot;
		f->gaitClipCount += 1;
	}

	if ( f->gaitClipCount == 2 && f->gaitClips[1].speed < f->gaitClips[0].speed )
	{
		nbGaitClip slower = f->gaitClips[1];
		f->gaitClips[1] = f->gaitClips[0];
		f->gaitClips[0] = slower;
	}
	f->gaitClipRoot = f->gaitClipCount > 0 && moveRoot;
}

// The time in a clip of the gait at a phase of the gait of the figure, see nbGaitClip: the touchdown of the left foot
// at phase 0, of the right foot at phase 0.5, and evenly between them
static float nbGaitClipTime( const nbGaitClip* gait, float phase )
{
	float between = nbWrapTime( gait->down[1] - gait->down[0], gait->cycle );
	float time = phase < 0.5f ? 2.0f * phase * between : between + ( 2.0f * phase - 1.0f ) * ( gait->cycle - between );
	return gait->start + nbWrapTime( gait->down[0] + time, gait->cycle );
}

// Sample a clip of the gait at a phase of the gait into a pose and a root, the way of the root over the ground taken
// out
static void nbSampleGaitClip( const nbFigure* f, const nbGaitClip* gait, float phase, b3Quat* pose, b3Transform* root )
{
	float time = nbGaitClipTime( gait, phase );
	nbSampleFigureClip( f, &gait->clip, time, pose, root );
	root->p = b3MulSub( root->p, time - gait->start, gait->drift );
}

// Whether the clips of the gait of a figure move its body: it has some, and no clip of the program plays
static bool nbGaitClipped( const nbFigure* f )
{
	return f->gaitClipCount > 0 && f->playing == false;
}

// Whether they move its root too: all of them move the root, see nbClipDef::moveRoot
static bool nbGaitClipRooted( const nbFigure* f )
{
	return nbGaitClipped( f ) && f->gaitClipRoot;
}

// The root of a clip of the gait that moves the root at a phase of the gait, its way over the ground taken out
static b3Transform nbSampleGaitRoot( const nbGaitClip* gait, float phase )
{
	float time = nbGaitClipTime( gait, phase );
	float alpha;
	int segment = nbClipSegment( gait->clip.times, sizeof( float ), gait->clip.keyframeCount, true, time, &alpha );
	b3Transform root = nbBlendRoots( gait->clip.roots[segment], gait->clip.roots[segment + 1], alpha );
	root.p = b3MulSub( root.p, time - gait->start, gait->drift );
	return root;
}

// The root of the clips of the gait of a figure, all of which move the root, at a phase of the gait, the slower blended
// to the faster as far as in this update, see nbGaitClipPose
static b3Transform nbGaitClipRoot( const nbFigure* f, float phase )
{
	b3Transform root = nbSampleGaitRoot( f->gaitClips, phase );
	if ( f->gaitClipFast > 0.0f )
	{
		root = nbBlendRoots( root, nbSampleGaitRoot( f->gaitClips + 1, phase ), f->gaitClipFast );
	}
	return root;
}

void nbSampleClip( const nbClipDef* clip, float time, b3Quat* rotations, b3Transform* root, int capacity )
{
	if ( clip == NULL || rotations == NULL || capacity < clip->boneCount || clip->keyframeCount <= 0 )
	{
		return;
	}

	float alpha;
	int count = clip->keyframeCount;
	int segment = nbClipSegment( &clip->keyframes[0].time, sizeof( nbKeyframe ), count, clip->loop, time, &alpha );
	const nbKeyframe* from = clip->keyframes + segment;
	const nbKeyframe* to = clip->keyframes + ( count > 1 ? segment + 1 : segment );
	for ( int i = 0; i < clip->boneCount; ++i )
	{
		b3Quat a = from->rotations != NULL ? b3NormalizeQuat( from->rotations[i] ) : b3Quat_identity;
		b3Quat b = to->rotations != NULL ? b3NormalizeQuat( to->rotations[i] ) : b3Quat_identity;
		rotations[i] = b3NLerp( a, b, alpha );
	}
	if ( root != NULL )
	{
		b3Transform a = { from->root.p, b3NormalizeQuat( from->root.q ) };
		b3Transform b = { to->root.p, b3NormalizeQuat( to->root.q ) };
		*root = nbBlendRoots( a, b, alpha );
	}
}

int nbMakeHumanoidClip( nbFigureClip which, float height, nbKeyframe* keyframes, b3Quat* rotations, int capacity,
						nbClipDef* clip )
{
	if ( keyframes == NULL || rotations == NULL || clip == NULL || (int)which < 0 || which >= nb_figureClipCount ||
		 height <= 0.0f )
	{
		return 0;
	}
	return nbBuildHumanoidClip( which, height, keyframes, rotations, capacity, clip );
}

//----------------------------------------------------------------------------------------------------------------------
// Lookup

static nbFigure* nbGetFigureFromId( nbFigureId id, nbWorld** worldOut )
{
	if ( id.index1 < 1 || id.world0 >= NB_MAX_WORLDS )
	{
		return NULL;
	}

	// The lookup that waits for steps on a thread of their own
	nbWorld* world = nbGetWorld( id.world0 );
	if ( world->inUse == false || id.index1 > world->figures.count )
	{
		return NULL;
	}

	nbFigure* f = world->figures.data + ( id.index1 - 1 );
	if ( f->isFree || f->generation != id.generation )
	{
		return NULL;
	}

	if ( worldOut != NULL )
	{
		*worldOut = world;
	}
	return f;
}

static nbFigureId nbMakeFigureId( const nbWorld* world, int index )
{
	return (nbFigureId){ index + 1, (uint16_t)world->worldIndex, world->figures.data[index].generation };
}

// Remember the bone of a body, by the index of the body in Box3D
static void nbMapBone( nbWorld* world, b3BodyId bodyId, int figure, int bone )
{
	int bodyIndex = bodyId.index1 - 1;
	if ( bodyIndex >= world->bodyToBone.count )
	{
		nbArray_Reserve( world->bodyToBone, bodyIndex + 1 );
		for ( int k = world->bodyToBone.count; k <= bodyIndex; ++k )
		{
			world->bodyToBone.data[k] = (nbBoneRef){ NB_NULL_INDEX, NB_NULL_INDEX };
		}
		world->bodyToBone.count = bodyIndex + 1;
	}
	world->bodyToBone.data[bodyIndex] = (nbBoneRef){ figure, bone };
}

static void nbUnmapBone( nbWorld* world, b3BodyId bodyId )
{
	int bodyIndex = bodyId.index1 - 1;
	if ( 0 <= bodyIndex && bodyIndex < world->bodyToBone.count )
	{
		world->bodyToBone.data[bodyIndex] = (nbBoneRef){ NB_NULL_INDEX, NB_NULL_INDEX };
	}
}

int nbFigureOfBody( const nbWorld* world, b3BodyId bodyId, int* bone )
{
	int bodyIndex = bodyId.index1 - 1;
	if ( bodyIndex < 0 || bodyIndex >= world->bodyToBone.count )
	{
		return NB_NULL_INDEX;
	}

	nbBoneRef ref = world->bodyToBone.data[bodyIndex];
	if ( ref.figure == NB_NULL_INDEX )
	{
		return NB_NULL_INDEX;
	}

	// The body at the index may be another one by now
	b3BodyId stored = world->figures.data[ref.figure].bones[ref.bone].bodyId;
	if ( stored.index1 != bodyId.index1 || stored.generation != bodyId.generation )
	{
		return NB_NULL_INDEX;
	}

	if ( bone != NULL )
	{
		*bone = ref.bone;
	}
	return ref.figure;
}

//----------------------------------------------------------------------------------------------------------------------
// Poses

// The world transforms of the bones of a pose with the root at a world transform
nbFigureId nbFigureIdOf( const nbWorld* world, int index )
{
	return nbMakeFigureId( world, index );
}

static void nbComputePose( const nbFigure* f, b3WorldTransform root, const b3Quat* pose, b3WorldTransform* transforms )
{
	transforms[0] = root;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		b3Transform local = { bone->rest.p, b3NormalizeQuat( b3MulQuat( bone->rest.q, pose[i] ) ) };
		transforms[i] = b3MulWorldTransforms( transforms[bone->parent], local );
	}
}

// The pose the bodies have: the rotation of every bone against its rest pose, and the root as it is
static void nbReadPose( const nbFigure* f, b3Quat* pose, b3WorldTransform* root )
{
	*root = b3Body_GetTransform( f->bones[0].bodyId );
	pose[0] = b3Quat_identity;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		b3Quat parent = b3Body_GetRotation( f->bones[bone->parent].bodyId );
		b3Quat rotation = b3Body_GetRotation( bone->bodyId );
		pose[i] = b3NormalizeQuat( b3InvMulQuat( b3MulQuat( parent, bone->rest.q ), rotation ) );
	}
}

// Move the bodies of a kinematic figure toward its targets in the next step
static void nbDriveBodies( nbFigure* f, float timeStep )
{
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Body_SetTargetTransform( f->bones[i].bodyId, f->targets[i], timeStep, true );
	}
}

// Mass weighted average velocity of the bones. Every bone keeps its own as the velocity the next step begins with, see
// nbPassFigureThrough.
static b3Vec3 nbAverageVelocity( nbFigure* f )
{
	b3Vec3 sum = b3Vec3_zero;
	float mass = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Vec3 velocity = b3Body_GetLinearVelocity( f->bones[i].bodyId );
		f->bones[i].stepVelocity = velocity;
		sum = b3MulAdd( sum, f->bones[i].mass, velocity );
		mass += f->bones[i].mass;
	}
	return mass > 0.0f ? b3MulSV( 1.0f / mass, sum ) : b3Vec3_zero;
}

// The velocities of the bones as the next step begins with them
static void nbReadVelocities( nbFigure* f )
{
	for ( int i = 0; i < f->boneCount; ++i )
	{
		f->bones[i].stepVelocity = b3Body_GetLinearVelocity( f->bones[i].bodyId );
	}
}

// The up of the world
static b3Vec3 nbUpOf( const nbWorld* world )
{
	b3Vec3 gravity = b3World_GetGravity( world->physicsWorld );
	return b3LengthSquared( gravity ) > 0.0f ? b3Neg( b3Normalize( gravity ) ) : b3Vec3_axisY;
}

// The frame of a figure: the point on the ground and the heading about up, where up is the y axis of the frame. Up is
// taken along the gravity of the world, turned from y the shortest way.
b3WorldTransform nbMakeFrame( const nbWorld* world, b3Pos position, float heading )
{
	b3Vec3 up = nbUpOf( world );
	b3Quat tilt = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, up );
	return (b3WorldTransform){ position, b3NormalizeQuat( b3MulQuat( tilt, nbHeadingQuat( heading ) ) ) };
}

//----------------------------------------------------------------------------------------------------------------------
// Queries

// What a query of a figure leaves out: its own bones, the other figures if asked, if asked all but fixed bodies, and if
// asked what a cast starts in
typedef struct nbFigureQuery
{
	const nbWorld* world;
	int figure;
	bool skipFigures;
	bool hit;
	float fraction;
	b3Vec3 normal;
	b3Pos point;
	b3ShapeId shapeId;
	bool staticOnly;
	bool skipOverlaps;
} nbFigureQuery;

b3QueryFilter nbFigureQueryFilter( const nbWorld* world, int index )
{
	b3QueryFilter filter = b3DefaultQueryFilter();
#if defined( B3_HAS_QUERY_EXCLUDE_GROUP )
	filter.excludeGroup = world->figures.data[index].groupIndex;
#else
	NB_UNUSED( world );
	NB_UNUSED( index );
#endif
	return filter;
}

static bool nbSkipShape( const nbFigureQuery* query, b3ShapeId shapeId )
{
	int other = nbFigureOfBody( query->world, b3Shape_GetBody( shapeId ), NULL );
	return other != NB_NULL_INDEX && ( other == query->figure || query->skipFigures );
}

static float nbClosestHitFcn( b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId,
							  int triangleIndex, int childIndex, void* context )
{
	NB_UNUSED( userMaterialId );
	NB_UNUSED( triangleIndex );
	NB_UNUSED( childIndex );
	nbFigureQuery* query = context;
	if ( b3Shape_IsSensor( shapeId ) || nbSkipShape( query, shapeId ) ||
		 ( query->staticOnly && b3Body_GetType( b3Shape_GetBody( shapeId ) ) != b3_staticBody ) ||
		 ( query->skipOverlaps && fraction == 0.0f && b3LengthSquared( normal ) < 0.25f ) )
	{
		return -1.0f;
	}

	// The callbacks come in any order: the closest wins, and of two at the same distance the shape of the lower index
	if ( query->hit == false || fraction < query->fraction ||
		 ( fraction == query->fraction && shapeId.index1 < query->shapeId.index1 ) )
	{
		query->hit = true;
		query->fraction = fraction;
		query->normal = normal;
		query->point = point;
		query->shapeId = shapeId;
	}
	return fraction;
}

static bool nbAnyOverlapFcn( b3ShapeId shapeId, void* context )
{
	nbFigureQuery* query = context;
	if ( b3Shape_IsSensor( shapeId ) || nbSkipShape( query, shapeId ) ||
		 ( query->staticOnly && b3Body_GetType( b3Shape_GetBody( shapeId ) ) != b3_staticBody ) )
	{
		return true;
	}
	query->hit = true;
	return false;
}

// The ground under a point: cast down from above it, leaving out the figure and, if asked, every other figure
static nbFigureQuery nbFindGround( const nbWorld* world, int index, b3Pos point, b3Vec3 up, float above, float below )
{
	nbFigureQuery query = { world, index, true, false, 1.0f, b3Vec3_zero, point, b3_nullShapeId, false, false };
	b3Pos origin = b3OffsetPos( point, b3MulSV( above, up ) );
	b3World_CastRay( world->physicsWorld, origin, b3MulSV( -( above + below ), up ), nbFigureQueryFilter( world, index ),
					 nbClosestHitFcn, &query );
	return query;
}

// Does anything but the figure overlap a capsule, or if asked anything fixed?
static bool nbCapsuleBlocked( const nbWorld* world, int index, b3Pos origin, b3Vec3 a, b3Vec3 b, float radius, bool staticOnly )
{
	b3Vec3 points[2] = { a, b };
	b3ShapeProxy proxy = { points, 2, radius };
	nbFigureQuery query = { world, index, false, false, 1.0f, b3Vec3_zero, origin, b3_nullShapeId, staticOnly, false };
	b3World_OverlapShape( world->physicsWorld, origin, &proxy, nbFigureQueryFilter( world, index ), nbAnyOverlapFcn, &query );
	return query.hit;
}

//----------------------------------------------------------------------------------------------------------------------
// States

// Ask the muscles of the joint of a bone for a strength, see nbSetStrength and nbUpdateMuscles
void nbSetBoneSpring( nbFigure* f, int index, float strength, bool enable )
{
	f->bones[index].drive = enable ? b3MaxFloat( strength, 0.0f ) : 0.0f;
}

// The stiffness of the spring of the joint of a bone in cycles per second: it rises with the root of the activation of
// its muscles, and a joint that gives way after a hit keeps only a share of its stiffness while the figure catches
// itself, see nbSoftenJoints
static float nbSpringHertz( const nbFigure* f, const nbBone* bone )
{
	float hertz = sqrtf( b3MaxFloat( bone->activation, 0.0f ) ) * bone->hertz;
	if ( f->state == nb_figureBalancing && f->softness > 0.0f && bone->hitStiffness < 1.0f )
	{
		hertz *= sqrtf( 1.0f - f->softShare * ( 1.0f - bone->hitStiffness ) * f->softness );
	}
	return hertz;
}

// Give the spring of the joint of a bone a stiffness, if it changed
static void nbSetSpringHertz( nbBone* bone, float hertz )
{
	if ( hertz == bone->springHertz )
	{
		return;
	}
	if ( bone->joint == nb_hingeJoint )
	{
		b3RevoluteJoint_SetSpringHertz( bone->jointId, hertz );
	}
	else
	{
		b3SphericalJoint_SetSpringHertz( bone->jointId, hertz );
	}
	bone->springHertz = hertz;
}

// The spring of the joint of a bone at the activation of its muscles, its torque limit times a factor, or off. Box3D only
// hears what changed: a muscle that holds still asks nothing of it.
static void nbApplyMuscles( nbFigure* f, int index, bool on, float factor )
{
	nbBone* bone = f->bones + index;
	NB_ASSERT( bone->joint == nb_hingeJoint || bone->joint == nb_ballJoint );
	bool hinge = bone->joint == nb_hingeJoint;
	if ( on != bone->springOn )
	{
		if ( hinge )
		{
			b3RevoluteJoint_EnableSpring( bone->jointId, on );
		}
		else
		{
			b3SphericalJoint_EnableSpring( bone->jointId, on );
		}
		bone->springOn = on;
	}
	if ( on == false )
	{
		return;
	}

	nbSetSpringHertz( bone, nbSpringHertz( f, bone ) );

	float dampingRatio = b3MaxFloat( 0.0f, f->def.motors.dampingRatio ) * bone->damping;
	if ( f->state == nb_figureBalancing )
	{
		dampingRatio *= bone->catchDamping;
	}
	if ( dampingRatio != bone->springDamping )
	{
		if ( hinge )
		{
			b3RevoluteJoint_SetSpringDampingRatio( bone->jointId, dampingRatio );
		}
		else
		{
			b3SphericalJoint_SetSpringDampingRatio( bone->jointId, dampingRatio );
		}
		bone->springDamping = dampingRatio;
	}

#if NB_SPRING_TORQUE_LIMITS
	float maxTorque = b3MaxFloat( 0.0f, factor * bone->brace * bone->activation * f->def.motors.maxTorque * bone->strength );
	if ( maxTorque != bone->springTorque )
	{
		if ( hinge )
		{
			b3RevoluteJoint_SetMaxSpringTorque( bone->jointId, maxTorque );
		}
		else
		{
			b3SphericalJoint_SetMaxSpringTorque( bone->jointId, maxTorque );
		}
		bone->springTorque = maxTorque;
	}
#else
	NB_UNUSED( factor );
#endif
}

// How strong the muscles of a joint pull for the speed it turns at, after the curve of Hill (1938): the speed of the
// joint the way its spring pulled in the step before, positive while the muscles shorten. They pull the less the faster
// they shorten, nothing at the top speed of the joint, and more while they are stretched, up to the braking strength.
static float nbMuscleSpeedFactor( nbFigure* f, nbBone* bone, float timeStep )
{
#if NB_SPRING_TORQUE_LIMITS
	if ( f->def.motors.muscles == false || bone->maxSpeed <= 0.0f )
	{
		return 1.0f;
	}

	float along = 0.0f;
	if ( bone->joint == nb_hingeJoint )
	{
		float angle = b3RevoluteJoint_GetAngle( bone->jointId );
		float speed = timeStep > 0.0f ? ( angle - bone->hingeAngle ) / timeStep : 0.0f;
		bone->hingeAngle = angle;
		float torque = b3RevoluteJoint_GetSpringTorque( bone->jointId );
		along = torque > 0.0f ? speed : ( torque < 0.0f ? -speed : 0.0f );
	}
	else
	{
		b3Vec3 relative =
			b3Sub( b3Body_GetAngularVelocity( bone->bodyId ), b3Body_GetAngularVelocity( f->bones[bone->parent].bodyId ) );
		b3Vec3 torque = b3SphericalJoint_GetSpringTorque( bone->jointId );
		float length = b3Length( torque );
		along = length > 1.0e-6f ? b3Dot( relative, torque ) / length : 0.0f;
	}

	float v = along / bone->maxSpeed;
	if ( v >= 0.0f )
	{
		return b3MaxFloat( 0.0f, ( 1.0f - v ) / ( 1.0f + NB_HILL_CURVE * v ) );
	}
	float x = -v;
	return 1.0f + ( b3MaxFloat( f->def.motors.brakeStrength, 1.0f ) - 1.0f ) * x / ( x + NB_HILL_BRAKE_RISE );
#else
	NB_UNUSED( f );
	NB_UNUSED( bone );
	NB_UNUSED( timeStep );
	return 1.0f;
#endif
}

// The muscles of every joint of a figure with dynamic bones, in every update: their activation follows the strength
// asked of them as a first order lag, with a time to tense and one to relax, implicit so it never overshoots, the spring
// of the joint stays on while they are active, its stiffness and torque limit rise with the activation, and the torque
// limit follows the speed of the joint, see nbMuscleSpeedFactor. Without nbFigureMotors::muscles the activation is the
// strength asked and the torque limit holds at any speed.
static void nbUpdateMuscles( nbFigure* f, float timeStep )
{
	const nbFigureMotors* motors = &f->def.motors;
	bool motorsOn = motors->stiffness > 0.0f && motors->maxTorque > 0.0f;
	float tense = motors->muscles ? b3MaxFloat( motors->tenseTime, 0.0f ) : 0.0f;
	float relax = motors->muscles ? b3MaxFloat( motors->relaxTime, 0.0f ) : 0.0f;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		nbBone* bone = f->bones + i;
		if ( bone->joint == nb_fixedJoint )
		{
			continue;
		}

		float time = bone->drive > bone->activation ? tense : relax;
		if ( time > 0.0f && timeStep > 0.0f )
		{
			float rate = timeStep / time;
			bone->activation = ( bone->activation + rate * bone->drive ) / ( 1.0f + rate );
		}
		if ( fabsf( bone->activation - bone->drive ) < NB_ACTIVATION_SNAP || time <= 0.0f || timeStep <= 0.0f )
		{
			bone->activation = bone->drive;
		}

		float factor = nbMuscleSpeedFactor( f, bone, timeStep );
		nbApplyMuscles( f, i, motorsOn && bone->activation >= NB_MIN_STRENGTH, factor );
	}
}

// Ask the muscles of a figure with dynamic bones for a strength, or let them relax. They follow in the update, see
// nbUpdateMuscles, or at once, see nbActivateAtOnce.
static void nbSetStrength( nbFigure* f, float strength )
{
	bool enable = strength >= NB_MIN_STRENGTH && f->def.motors.stiffness > 0.0f && f->def.motors.maxTorque > 0.0f;
	if ( enable == false && f->springs == false )
	{
		return;
	}

	for ( int i = 1; i < f->boneCount; ++i )
	{
		nbSetBoneSpring( f, i, strength, enable );
	}
	f->springs = enable;
}

// The muscles of a figure take the strength asked of them at once: one that catches itself holds with the strength the
// balance asks for, and one that falls goes slack, see nbBeginBalancing and nbFall
static void nbActivateAtOnce( nbFigure* f )
{
	bool motorsOn = f->def.motors.stiffness > 0.0f && f->def.motors.maxTorque > 0.0f;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		nbBone* bone = f->bones + i;
		if ( bone->joint == nb_fixedJoint )
		{
			continue;
		}

		bone->activation = bone->drive;
		if ( bone->joint == nb_hingeJoint )
		{
			bone->hingeAngle = b3RevoluteJoint_GetAngle( bone->jointId );
		}
		nbApplyMuscles( f, i, motorsOn && bone->activation >= NB_MIN_STRENGTH, 1.0f );
	}
}

// The joints that gave way to a hit stiffen again: soft for the time of nbSoftHold, then back to their stiffness on a
// smooth step over NB_SOFT_BACK seconds, see nbSoftenJoints
static void nbUpdateSoftness( nbFigure* f, float timeStep )
{
	if ( f->softTime <= 0.0f )
	{
		f->softness = 0.0f;
		return;
	}

	f->softTime = b3MaxFloat( f->softTime - timeStep, 0.0f );
	float phase = b3ClampFloat( 1.0f - f->softTime / NB_SOFT_BACK, 0.0f, 1.0f );
	f->softness = 1.0f - phase * phase * ( 3.0f - 2.0f * phase );
}

// How long the joints that gave way to a hit stay soft before they stiffen again, see NB_SOFT_HARD_CUT
static float nbSoftHold( const nbFigure* f, b3Vec3 impulse )
{
	float hold = NB_SOFT_HOLD;
	if ( f->balancer.ownStrength && f->def.knockdownImpulse > 0.0f )
	{
		float hardness =
			( b3Length( impulse ) / f->def.knockdownImpulse - NB_SOFT_HARD_START ) / ( NB_SOFT_HARD_FULL - NB_SOFT_HARD_START );
		hold *= 1.0f - NB_SOFT_HARD_CUT * b3ClampFloat( hardness, 0.0f, 1.0f );
	}
	return hold;
}

// A hit of at least NB_SOFT_IMPULSE times the knockdown impulse on a bone whose joint gives way, see
// nbBoneDef::hitStiffness, softens all joints of a figure that catches itself that give way at once, by the share of the
// hit that comes from ahead or behind: the square of the cosine between the hit over the ground and the facing of the
// figure. The spine bends forward and back with the hit from the next step on. To the side its joints let it bend less
// than half as far, and a softer spine there let the figure fall where it catches itself with steps to the side, see
// "Rumpf gibt nach" in docs/Optimierungen.md. The torque limits stay, the figure catches itself with all its strength.
static void nbSoftenJoints( nbFigure* f, int index, b3Vec3 impulse )
{
	if ( f->state != nb_figureBalancing || f->bones[index].hitStiffness >= 1.0f ||
		 b3Length( impulse ) < NB_SOFT_IMPULSE * f->def.knockdownImpulse )
	{
		return;
	}

	const nbBalancer* b = &f->balancer;
	b3Vec3 flat = b3Sub( impulse, b3MulSV( b3Dot( impulse, b->up ), b->up ) );
	float ahead = b3Dot( flat, b->forward );
	float across = b3LengthSquared( flat );
	float share = across > 1.0e-12f ? ahead * ahead / across : 1.0f;
	if ( share <= 0.0f )
	{
		return;
	}

	f->softShare = share;
	f->softTime = nbSoftHold( f, impulse ) + NB_SOFT_BACK;
	nbUpdateSoftness( f, 0.0f );
	for ( int i = 1; i < f->boneCount; ++i )
	{
		nbBone* bone = f->bones + i;
		if ( bone->springOn && bone->hitStiffness < 1.0f )
		{
			nbSetSpringHertz( bone, nbSpringHertz( f, bone ) );
		}
	}
}

// Drive the joints of a ragdoll to a pose, within their limits
static void nbSetJointTargets( nbFigure* f, const b3Quat* pose )
{
	for ( int i = 1; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		if ( bone->joint == nb_fixedJoint )
		{
			continue;
		}

		b3Quat relative = nbJointRotation( bone, pose[i] );
		nbLimitJointRotation( bone, &relative );
		if ( bone->joint == nb_hingeJoint )
		{
			b3RevoluteJoint_SetTargetAngle( bone->jointId, b3GetTwistAngle( relative ) );
		}
		else
		{
			b3SphericalJoint_SetTargetRotation( bone->jointId, relative );
		}
	}
}

// The damping of the bones of a ragdoll
static void nbSetDamping( nbFigure* f, float damping )
{
	if ( damping == f->damping )
	{
		return;
	}
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Body_SetLinearDamping( f->bones[i].bodyId, damping );
		b3Body_SetAngularDamping( f->bones[i].bodyId, damping );
	}
	f->damping = damping;
}

// The bones become dynamic with the velocities they have, the springs at strength 1 from the inertia they have now
static void nbMakeDynamic( nbFigure* f )
{
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Body_SetType( f->bones[i].bodyId, b3_dynamicBody );
	}

	for ( int i = 1; i < f->boneCount; ++i )
	{
		nbBone* bone = f->bones + i;
		float stiffness = b3MaxFloat( 0.0f, f->def.motors.stiffness ) * bone->stiffness;
		b3Quat frameB = b3MulQuat( b3Body_GetRotation( bone->bodyId ), bone->jointFrame );
		bone->hertz = bone->joint == nb_fixedJoint ? 0.0f
												   : nbJointSpringHertz( f->bones[bone->parent].bodyId, bone->bodyId, frameB,
																		 bone->joint == nb_hingeJoint, stiffness, bone->armature );
	}
}

b3BodyId nbFigureAnchor( nbWorld* world )
{
	if ( b3Body_IsValid( world->figureAnchor ) == false )
	{
		b3BodyDef def = b3DefaultBodyDef();
		def.type = b3_staticBody;
		world->figureAnchor = b3CreateBody( world->physicsWorld, &def );
	}
	return world->figureAnchor;
}

// A figure stops following its clip, see nbBeginFollowing: the drives of its bones go and they may sleep again. They stay
// dynamic for the figure to catch itself or fall, or become kinematic again where they are.
static void nbEndFollowing( nbWorld* world, nbFigure* f, bool kinematic )
{
	if ( f->following == false )
	{
		return;
	}

	bool physicsAlive = b3World_IsValid( world->physicsWorld );
	for ( int i = 0; i < f->boneCount; ++i )
	{
		if ( physicsAlive && b3Joint_IsValid( f->followJoints[i] ) )
		{
			b3DestroyJoint( f->followJoints[i], false );
		}
		f->followJoints[i] = b3_nullJointId;

		// The springs of the joints go for still targets again, see nbFollowJoints
		const nbBone* bone = f->bones + i;
		if ( physicsAlive && i > 0 && bone->joint != nb_fixedJoint && b3Joint_IsValid( bone->jointId ) )
		{
			if ( bone->joint == nb_hingeJoint )
			{
				b3RevoluteJoint_SetTargetSpeed( bone->jointId, 0.0f );
			}
			else
			{
				b3SphericalJoint_SetTargetVelocity( bone->jointId, b3Vec3_zero );
			}
		}
		b3Body_EnableSleep( f->bones[i].bodyId, true );
		if ( kinematic )
		{
			b3Body_SetType( f->bones[i].bodyId, b3_kinematicBody );
		}
	}
	f->following = false;
	if ( kinematic )
	{
		f->springs = false;
	}
}

// The figure falls as an active ragdoll: its bones become dynamic with the velocities they have, slack for the stun
// time if a hit met it as a ragdoll or a blast threw it
static void nbFall( nbWorld* world, nbFigure* f, bool stunned )
{
	nbEndFollowing( world, f, false );
	f->pushed = false;
	f->armPush[0] = 0.0f;
	f->armPush[1] = 0.0f;
	bool wasRagdoll = f->state == nb_figureRagdoll;
	if ( wasRagdoll == false )
	{
		// A figure that caught itself has its bones dynamic already, it lets go of the upright hold
		if ( f->state == nb_figureBalancing )
		{
			nbEndBalance( f );
		}
		else
		{
			nbMakeDynamic( f );
		}

		if ( f->state == nb_figureGettingUp )
		{
			f->abortCount += 1;
		}
		f->knockdownCount += 1;
		f->state = nb_figureRagdoll;
		f->stateTime = 0.0f;
		f->restTime = 0.0f;
		f->fallTime = 0.0f;
		f->protect = 0.0f;
		f->brace = 0.0f;
		f->strength = 0.0f;
		f->wait = nb_waitMoving;
		f->flinch = b3Vec3_zero;
		f->flinchRate = b3Vec3_zero;
		nbStopSwings( f );
		f->balancing = false;
		f->swingFoot = -1;
		f->footWeight = 0.0f;
		f->shiftLeft = 0.0f;
		f->springs = true;
		nbSetStrength( f, 0.0f );
		nbActivateAtOnce( f );
		f->damping = -1.0f;
		nbSetDamping( f, 0.0f );
	}

	if ( stunned )
	{
		f->stun = b3MaxFloat( f->stun, f->def.motors.stunTime );
	}
	b3Body_SetAwake( f->bones[0].bodyId, true );
	nbReadVelocities( f );
}

// Where a figure without room moves to get up, see nbFindRoom: how far its body moves over the ground and how far it turns
// about its pelvis, in radians about up
typedef struct nbShift
{
	b3Vec3 offset;
	float turn;
} nbShift;

// A lying figure starts to get up: the pose it lies in, the frame it gets up in and the clip, and its bones kinematic. A
// figure that moves to get up holds the pose it lies in while its root moves and turns there, see nbUpdateGettingUp.
static void nbStartGetUp( nbWorld* world, int index, nbFigure* f, b3Pos ground, b3Vec3 up, nbShift shift )
{
	NB_UNUSED( index );
	nbReadPose( f, f->lyingPose, &f->lyingRoot );
	f->shiftFrom = f->lyingRoot;
	f->shiftTurn = shift.turn;
	f->shiftLeft = 0.0f;
	if ( b3LengthSquared( shift.offset ) > 0.0f || shift.turn != 0.0f )
	{
		f->lyingRoot.p = b3OffsetPos( f->lyingRoot.p, shift.offset );
		f->lyingRoot.q = b3NormalizeQuat( b3MulQuat( b3MakeQuatFromAxisAngle( up, shift.turn ), f->lyingRoot.q ) );
		f->shiftTime = b3MaxFloat( NB_SHIFT_TIME, b3MaxFloat( b3Length( shift.offset ) / NB_SHIFT_SPEED,
															  fabsf( shift.turn ) / NB_TURN_SPEED ) );
		f->shiftLeft = f->shiftTime;
		f->shiftCount += 1;
	}

	// The heading along the spine, from the pelvis toward the head, as the clips lie with the head ahead
	b3Vec3 spine = b3RotateVector( f->lyingRoot.q, b3Vec3_axisY );
	b3Vec3 front = b3RotateVector( f->lyingRoot.q, b3Vec3_axisZ );
	b3Vec3 flat = b3MulSub( spine, b3Dot( spine, up ), up );
	if ( b3LengthSquared( flat ) < 1.0e-4f )
	{
		// Standing on its head or sitting: the front decides
		flat = b3MulSub( front, b3Dot( front, up ), up );
	}

	b3WorldTransform frame = nbMakeFrame( world, ground, 0.0f );
	b3Vec3 local = b3InvRotateVector( frame.q, flat );
	f->heading = b3Atan2( local.x, local.z );
	f->frame = nbMakeFrame( world, ground, f->heading );
	f->onBack = b3Dot( front, up ) > 0.0f;
	f->getUpClip = f->onBack ? nb_clipGetUpBack : nb_clipGetUpFront;
	f->getUpTime = 0.0f;

	// Every bone turns from how it lies toward the clip the way that is shorter now, see nbUpdateGettingUpPose: the pose
	// it lies in on the side of the start of the clip, which the fade pose keeps
	b3Transform clipRoot;
	nbSampleFigureClip( f, f->clips[f->getUpClip], 0.0f, f->fadePose, &clipRoot );
	for ( int i = 0; i < f->boneCount; ++i )
	{
		if ( b3DotQuat( f->lyingPose[i], f->fadePose[i] ) < 0.0f )
		{
			f->lyingPose[i] = b3NegateQuat( f->lyingPose[i] );
		}
	}

	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Body_SetType( f->bones[i].bodyId, b3_kinematicBody );
	}
	nbSetDamping( f, 0.0f );
	f->springs = false;
	f->state = nb_figureGettingUp;
	f->stateTime = 0.0f;
	f->wait = nb_waitNone;
}

// Blend a pose in from another one over a time, see nbFigure::fadePose
static void nbStartFade( nbFigure* f, float duration )
{
	memcpy( f->fadePose, f->pose, sizeof( b3Quat ) * (size_t)f->boneCount );
	f->fadeRoot = f->root;
	f->fadeTime = 0.0f;
	f->fadeDuration = duration;
	f->fadeLegs = false;
}

// The feet where the clip puts them, on the ground of the frame
static void nbResetFeet( nbFigure* f )
{
	for ( int k = 0; k < 2; ++k )
	{
		nbFoot* foot = f->feet + k;
		foot->tilt = b3Quat_identity;
		foot->fromTilt = b3Quat_identity;
		foot->targetTilt = b3Quat_identity;
		foot->lift = 0.0f;
		foot->lean = b3Quat_identity;
		foot->down = 1.0f;
		foot->hasGround = false;
		foot->cast = false;
	}
	f->feetFlat = false;
	f->balancing = false;
	f->swingFoot = -1;
	f->footWeight = 0.0f;
}

// A figure that got up stands: the frame moves to where the clip left the root, the stand clip blends in
static void nbStand( nbWorld* world, nbFigure* f )
{
	// Under the root as the get-up clip left it, turned as far as it turned about up
	b3Transform last = f->root;
	b3Vec3 ahead = b3RotateVector( last.q, b3Vec3_axisZ );
	float turn = b3Atan2( ahead.x, ahead.z );
	b3Pos ground = b3TransformWorldPoint( f->frame, (b3Vec3){ last.p.x, 0.0f, last.p.z } );
	f->heading += turn;
	f->frame = nbMakeFrame( world, ground, f->heading );
	f->root = (b3Transform){ { 0.0f, last.p.y, 0.0f }, b3NormalizeQuat( b3MulQuat( nbHeadingQuat( -turn ), last.q ) ) };

	nbStartFade( f, NB_STAND_FADE );
	nbResetFeet( f );
	nbResetSteering( f );
	f->state = nb_figureAnimated;
	f->stateTime = 0.0f;
	f->playing = false;
	f->clipTime = 0.0f;
	f->clipSpeed = 1.0f;
	f->getUpCount += 1;
}

// A standing humanoid catches itself after a hard hit, see figure_balance.c: its bones dynamic with the velocities they
// have, the joints at full strength toward the pose it stands in, the upright hold on. Any other figure falls.
static void nbBeginBalancing( nbWorld* world, nbFigure* f )
{
	nbEndFollowing( world, f, false );
	if ( f->canBalance == false )
	{
		nbFall( world, f, false );
		return;
	}

	int index = (int)( f - world->figures.data );
	b3Quat pose[nb_humanoidBoneCount];
	b3WorldTransform root;
	nbReadPose( f, pose, &root );
	nbMakeDynamic( f );
	f->state = nb_figureBalancing;
	f->stateTime = 0.0f;
	f->flinch = b3Vec3_zero;
	f->flinchRate = b3Vec3_zero;
	f->softShare = 0.0f;
	f->softness = 0.0f;
	f->softTime = 0.0f;
	nbStopSwings( f );
	f->balancing = false;
	f->swingFoot = -1;
	f->footWeight = 0.0f;
	f->stun = 0.0f;
	f->strength = 1.0f;
	f->springs = true;
	nbSetStrength( f, 1.0f );
	f->damping = -1.0f;
	nbSetDamping( f, 0.0f );
	nbBeginBalance( world, index, f );
	nbActivateAtOnce( f );
	nbSetJointTargets( f, pose );
	b3Body_SetAwake( f->bones[0].bodyId, true );
	nbReadVelocities( f );
}

// A humanoid with dynamic bones stands animated again where it stands: the frame on the ground under the middle of its
// ankles, at the height of the ground under its feet, facing along a direction over the ground, and its clip blends in
// from the pose it stands in over a time, as after getting up, if asked its legs too. A figure that catches itself lets
// go of its upright hold.
static void nbStandWhereItIs( nbWorld* world, nbFigure* f, float groundHeight, b3Vec3 forward, float fade, bool fadeLegs )
{
	b3Vec3 up = nbUpOf( world );
	b3WorldTransform pelvis;
	nbReadPose( f, f->pose, &pelvis );

	b3Vec3 ankles = b3MulSV( 0.5f, b3Add( b3ToVec3( b3Body_GetPosition( f->bones[nb_humanoidFootLeft].bodyId ) ),
										   b3ToVec3( b3Body_GetPosition( f->bones[nb_humanoidFootRight].bodyId ) ) ) );
	b3Vec3 ground = b3MulAdd( ankles, groundHeight - b3Dot( ankles, up ), up );
	b3WorldTransform level = nbMakeFrame( world, b3ToPos( ground ), 0.0f );
	b3Vec3 ahead = b3InvRotateVector( level.q, forward );
	f->heading = b3Atan2( ahead.x, ahead.z );
	f->frame = nbMakeFrame( world, b3ToPos( ground ), f->heading );
	f->root = b3InvMulWorldTransforms( f->frame, pelvis );

	nbEndBalance( f );
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Body_SetType( f->bones[i].bodyId, b3_kinematicBody );
	}
	nbSetDamping( f, 0.0f );
	f->springs = false;
	f->strength = 0.0f;
	nbStartFade( f, fade );
	f->fadeLegs = fadeLegs;
	nbResetFeet( f );
	nbResetSteering( f );
	f->state = nb_figureAnimated;
	f->stateTime = 0.0f;

	// The feet stay where they stand, and step under the body to where the clip puts them, one after the other, as after a
	// push it caught over its feet, see nbPush
	f->feet[0].plant = b3Body_GetPosition( f->bones[nb_humanoidFootLeft].bodyId );
	f->feet[1].plant = b3Body_GetPosition( f->bones[nb_humanoidFootRight].bodyId );
	f->balancing = true;
	f->footWeight = 1.0f;
	f->swingFoot = -1;
	f->swayOffset = b3Vec3_zero;
	f->swayTime = 0.0f;
	f->swayDuration = 0.0f;
}

// A figure that caught itself stands animated again where it stands, facing where its feet face
static void nbSettle( nbWorld* world, nbFigure* f )
{
	nbStandWhereItIs( world, f, nbBalanceGround( f ), nbBalanceForward( f ), NB_STAND_FADE, false );
	f->catchCount += 1;
	f->pushed = false;
}

//----------------------------------------------------------------------------------------------------------------------
// Balance

// Steps to catch a push after the roadmap of the game, 26.14 and 28.3. A hit that does not knock a standing humanoid down
// gives its body a velocity over the ground. As a pendulum as long as its stand height it would come to rest over the
// capture point, the velocity over the frequency of the pendulum ahead of its center of mass. Where that point lies
// within the soles of its feet, shrunk about the center of mass to the part it trusts, it stands and only flinches. The
// humanoid stands over its ankles, 3 cm before its heels and 23 cm behind its toes, so it steps back from less than to
// its side, and to its side from less than forward. Else its body moves to the capture point,
// slowing down evenly in two over that frequency, and its feet step one after the other to where the clip puts them under
// the body there: the one toward the push first, each held where it stands while the other is in the air, so it does not
// slide. A step needs ground under its end, flat enough and not moving fast, and the body no wall in its way. Without, or
// for a push beyond the reach, the figure falls instead. It decides when a push comes and when a foot lands, never in
// between, so it does not flutter.

// The scale of a humanoid against the one of 1.8 m
static float nbFigureScale( const nbFigure* f )
{
	return f->def.standHeight / NB_HUMANOID_STAND;
}

// Only the humanoid steps, its legs and feet are known
static bool nbCanStep( const nbFigure* f )
{
	return f->boneCount == nb_humanoidBoneCount && f->def.stepReach > 0.0f;
}

// The transforms of the bones of a pose in the frame of the figure, from the root there
static void nbFramePose( const nbFigure* f, b3Transform root, const b3Quat* pose, b3Transform* transforms )
{
	transforms[0] = root;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		b3Transform local = { bone->rest.p, b3NormalizeQuat( b3MulQuat( bone->rest.q, pose[i] ) ) };
		transforms[i] = b3MulTransforms( transforms[bone->parent], local );
	}
}

// The transforms of the root and the legs of a pose of the humanoid in the frame of the figure, from the root there
static void nbFrameLegs( const nbFigure* f, b3Transform root, const b3Quat* pose, b3Transform* transforms )
{
	transforms[0] = root;
	for ( int i = nb_humanoidThighLeft; i <= nb_humanoidFootRight; ++i )
	{
		const nbBone* bone = f->bones + i;
		b3Transform local = { bone->rest.p, b3NormalizeQuat( b3MulQuat( bone->rest.q, pose[i] ) ) };
		transforms[i] = b3MulTransforms( transforms[bone->parent], local );
	}
}

// The center of mass of a pose in the frame, from the centers of its bones
static b3Vec3 nbFrameCenter( const nbFigure* f, const b3Transform* transforms )
{
	b3Vec3 sum = b3Vec3_zero;
	float mass = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		b3Vec3 center = bone->shape == nb_capsuleBone ? b3MulSV( 0.5f, b3Add( bone->start, bone->end ) ) : bone->center;
		sum = b3MulAdd( sum, bone->mass, b3TransformPoint( transforms[i], center ) );
		mass += bone->mass;
	}
	return b3MulSV( 1.0f / mass, sum );
}

// Twice the area of the triangle o, a, b on the ground of the frame, positive where b lies left of o to a
static float nbCrossOnGround( const b3Vec3* o, const b3Vec3* a, const b3Vec3* b )
{
	return ( a->x - o->x ) * ( b->z - o->z ) - ( a->z - o->z ) * ( b->x - o->x );
}

// How far a point of the ground of the frame lies inside the soles of the feet that stand on it, seen from above,
// negative outside: the corners of the feet the clip holds down and their convex hull, by Andrew's monotone chain
static float nbInsideSoles( const nbFigure* f, const b3Transform* transforms, float x, float z )
{
	b3Vec3 points[2 * NB_BONE_POINTS];
	int count = 0;
	const int feet[2] = { nb_humanoidFootLeft, nb_humanoidFootRight };
	for ( int k = 0; k < 2; ++k )
	{
		const nbBone* bone = f->bones + feet[k];
		nbBoneDef def = nbDefaultBoneDef();
		def.shape = bone->shape;
		def.center = bone->center;
		def.halfExtents = bone->halfExtents;
		def.radius = bone->radius;
		b3Vec3 corners[NB_BONE_POINTS];
		int cornerCount = nbBonePoints( &def, corners );
		if ( cornerCount == 0 )
		{
			corners[0] = bone->start;
			corners[1] = bone->end;
			cornerCount = 2;
		}

		if ( f->feet[k].down < 0.5f )
		{
			continue;
		}
		for ( int c = 0; c < cornerCount; ++c )
		{
			corners[c] = b3TransformPoint( transforms[feet[k]], corners[c] );
		}
		for ( int c = 0; c < cornerCount; ++c )
		{
			points[count++] = (b3Vec3){ corners[c].x, 0.0f, corners[c].z };
		}
	}

	// By x, then by z
	for ( int i = 1; i < count; ++i )
	{
		b3Vec3 p = points[i];
		int j = i - 1;
		while ( j >= 0 && ( points[j].x > p.x || ( points[j].x == p.x && points[j].z > p.z ) ) )
		{
			points[j + 1] = points[j];
			j -= 1;
		}
		points[j + 1] = p;
	}

	// The lower and the upper chain, counterclockwise on the ground
	b3Vec3 hull[4 * NB_BONE_POINTS];
	int size = 0;
	for ( int i = 0; i < count; ++i )
	{
		while ( size >= 2 && nbCrossOnGround( hull + size - 2, hull + size - 1, points + i ) <= 0.0f )
		{
			size -= 1;
		}
		hull[size++] = points[i];
	}
	int lower = size + 1;
	for ( int i = count - 2; i >= 0; --i )
	{
		while ( size >= lower && nbCrossOnGround( hull + size - 2, hull + size - 1, points + i ) <= 0.0f )
		{
			size -= 1;
		}
		hull[size++] = points[i];
	}
	size -= 1;
	if ( size < 3 )
	{
		return -1.0f;
	}

	b3Vec3 point = { x, 0.0f, z };
	float inside = FLT_MAX;
	for ( int i = 0; i < size; ++i )
	{
		const b3Vec3* a = hull + i;
		const b3Vec3* b = hull + ( i + 1 ) % size;
		float length = sqrtf( ( b->x - a->x ) * ( b->x - a->x ) + ( b->z - a->z ) * ( b->z - a->z ) );
		if ( length > 0.0f )
		{
			inside = b3MinFloat( inside, nbCrossOnGround( a, b, &point ) / length );
		}
	}
	return inside;
}

// The velocity the body of a figure that catches a push still moves with, slowing down evenly
static b3Vec3 nbSwayVelocity( const nbFigure* f )
{
	if ( f->balancing == false || f->swayTime >= f->swayDuration )
	{
		return b3Vec3_zero;
	}
	float u = f->swayTime / f->swayDuration;
	return b3MulSV( 2.0f * ( 1.0f - u ) / f->swayDuration, f->swayOffset );
}

// How far the push still carries the body
static b3Vec3 nbSwayLeft( const nbFigure* f )
{
	float u = f->swayDuration > 0.0f ? b3MinFloat( f->swayTime / f->swayDuration, 1.0f ) : 1.0f;
	return b3MulSV( ( 1.0f - u ) * ( 1.0f - u ), f->swayOffset );
}

// Does a fixed body stand in the way of the body of a figure that a push carries along an offset from a point of its
// frame? What moves, the figure pushes aside.
static bool nbBodyBlocked( const nbWorld* world, int index, const nbFigure* f, b3Vec3 center, b3Vec3 offset, b3Vec3 up,
						   float scale )
{
	float length = b3Length( offset );
	if ( length < 1.0e-6f )
	{
		return false;
	}

	b3Pos origin = b3TransformWorldPoint( f->frame, (b3Vec3){ center.x, 0.0f, center.z } );
	b3Vec3 points[2] = { b3MulSV( NB_BODY_LOW * scale, up ), b3MulSV( NB_BODY_HIGH * scale, up ) };
	b3ShapeProxy proxy = { points, 2, NB_BODY_RADIUS * scale };
	b3Vec3 translation = b3MulSV( ( length + NB_BODY_MARGIN * scale ) / length, offset );
	nbFigureQuery query = { world, index, true, false, 1.0f, b3Vec3_zero, origin, b3_nullShapeId, true, false };
	b3World_CastShape( world->physicsWorld, origin, &proxy, translation, nbFigureQueryFilter( world, index ), nbClosestHitFcn,
					   &query );
	return query.hit;
}

// Where the clip puts a foot of the humanoid in the frame: its ankle, the middle of its sole, its rotation, and how far it
// holds the foot down, 1 while the sole is near the ground of the frame and 0 once it lifts it 10 cm
typedef struct nbClipFoot
{
	b3Vec3 ankle;
	b3Vec3 sole;
	b3Quat rotation;
	float weight;
} nbClipFoot;

static nbClipFoot nbGetClipFoot( const nbFigure* f, const b3Transform* transforms, int foot, float scale )
{
	// The lowest point of the shape, along the axes of the bone, for a box with its edges rounded by its radius
	const nbBone* bone = f->bones + foot;
	b3Transform transform = transforms[foot];
	float lowest;
	b3Vec3 center;
	if ( bone->shape == nb_capsuleBone )
	{
		b3Vec3 start = b3TransformPoint( transform, bone->start );
		b3Vec3 end = b3TransformPoint( transform, bone->end );
		lowest = b3MinFloat( start.y, end.y ) - bone->radius;
		center = b3MulSV( 0.5f, b3Add( start, end ) );
	}
	else
	{
		b3Vec3 h = bone->halfExtents;
		float round = bone->shape == nb_roundedBoxBone ? b3ClampFloat( bone->radius, 0.0f, 0.95f * b3MinFloat( h.x, b3MinFloat( h.y, h.z ) ) ) : 0.0f;
		b3Vec3 x = b3RotateVector( transform.q, b3Vec3_axisX );
		b3Vec3 y = b3RotateVector( transform.q, b3Vec3_axisY );
		b3Vec3 z = b3RotateVector( transform.q, b3Vec3_axisZ );
		center = b3TransformPoint( transform, bone->center );
		lowest = center.y - fabsf( x.y ) * ( h.x - round ) - fabsf( y.y ) * ( h.y - round ) - fabsf( z.y ) * ( h.z - round ) - round;
	}

	nbClipFoot clip;
	clip.ankle = transform.p;
	clip.sole = (b3Vec3){ center.x, lowest, center.z };
	clip.rotation = transform.q;
	clip.weight = b3ClampFloat( ( NB_SOLE_LIFTED * scale - lowest ) / ( ( NB_SOLE_LIFTED - NB_SOLE_GAP ) * scale ), 0.0f, 1.0f );
	return clip;
}

// The ankle of a foot whose sole the ground lifts and tilts, turned about the middle of the sole
static b3Vec3 nbTiltedAnkle( const nbClipFoot* clip, float lift, b3Quat tilt )
{
	b3Vec3 sole = { clip->sole.x, clip->sole.y + lift, clip->sole.z };
	return b3Add( sole, b3RotateVector( tilt, b3Sub( clip->ankle, clip->sole ) ) );
}

// A rotation of at most an angle, about the same axis
static b3Quat nbLimitRotation( b3Quat rotation, float limit )
{
	float angle;
	b3Vec3 axis = b3GetAxisAngle( &angle, b3NormalizeQuat( rotation ) );
	if ( angle <= limit || b3LengthSquared( axis ) == 0.0f )
	{
		return rotation;
	}
	return b3MakeQuatFromAxisAngle( axis, limit );
}

// The ground under the middle of a sole in the world: a sphere cast down from above it finds it, flat enough and not
// moving fast, with nothing that the cast starts in. A point of it and its normal, false where there is none. Where the
// sole was guessed as far as guess over the ground under the frame, from the slope of the way, the cast goes on as much
// farther down, or starts as much higher up below it. It starts as much higher as it may go deeper, so a foot finds a
// tread up stairs ahead while the capsule still runs on level ground, and where it finds nothing, it goes on deeper.
static bool nbCastFootGround( const nbWorld* world, int index, b3Pos sole, b3Vec3 up, float scale, float guess, float deeper,
							  b3Pos* point, b3Vec3* normal )
{
	float radius = NB_FOOT_RADIUS * scale;
	float above = NB_GROUND_ABOVE + radius;
	float length = NB_GROUND_ABOVE + NB_GROUND_BELOW;
	if ( guess != 0.0f )
	{
		above += b3MaxFloat( -guess, 0.0f );
		length += fabsf( guess );
	}
	above += deeper;
	length += deeper;
	b3Pos origin = b3OffsetPos( sole, b3MulSV( above, up ) );
	b3Vec3 center = b3Vec3_zero;
	b3ShapeProxy proxy = { &center, 1, radius };
	nbFigureQuery query = { world, index, true, false, 1.0f, b3Vec3_zero, origin, b3_nullShapeId, false, true };
	b3World_CastShape( world->physicsWorld, origin, &proxy, b3MulSV( -length, up ), nbFigureQueryFilter( world, index ),
					   nbClosestHitFcn, &query );
	if ( query.hit == false && deeper > 0.0f )
	{
		b3Pos below = b3OffsetPos( origin, b3MulSV( -length, up ) );
		b3World_CastShape( world->physicsWorld, below, &proxy, b3MulSV( -deeper, up ), nbFigureQueryFilter( world, index ),
						   nbClosestHitFcn, &query );
	}
	if ( query.hit == false || b3Dot( query.normal, up ) < NB_STEP_FLATNESS )
	{
		return false;
	}
	b3BodyId body = b3Shape_GetBody( query.shapeId );
	if ( b3Body_GetType( body ) != b3_staticBody && b3Length( b3Body_GetLinearVelocity( body ) ) > NB_GROUND_SPEED )
	{
		return false;
	}
	*point = query.point;
	*normal = query.normal;
	return true;
}

// How far a plane of the ground lifts the sole of a foot that the clip puts in a frame, at the middle of the sole, and
// how it tilts it: the turn from the up of the frame to its normal, in the frame
static void nbGroundUnderSole( b3WorldTransform frame, const nbClipFoot* clip, b3Pos point, b3Vec3 worldNormal, float* lift,
							   b3Quat* tilt )
{
	b3Vec3 normal = b3InvRotateVector( frame.q, worldNormal );
	b3Vec3 contact = b3InvTransformWorldPoint( frame, point );
	float height = contact.y - ( normal.x * ( clip->sole.x - contact.x ) + normal.z * ( clip->sole.z - contact.z ) ) / normal.y;
	*lift = height - clip->sole.y;
	*tilt = normal.x == 0.0f && normal.z == 0.0f ? b3Quat_identity
												 : nbLimitRotation( b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, normal ), NB_FOOT_TILT );
}

// The ground under the sole of a foot that the clip puts in a frame, see nbCastFootGround and nbGroundUnderSole
static bool nbFindFootGround( const nbWorld* world, int index, b3WorldTransform frame, const nbClipFoot* clip, float scale,
							  float guess, float deeper, float* lift, b3Quat* tilt )
{
	b3Pos point;
	b3Vec3 normal;
	b3Vec3 up = b3RotateVector( frame.q, b3Vec3_axisY );
	if ( nbCastFootGround( world, index, b3TransformWorldPoint( frame, clip->sole ), up, scale, guess, deeper, &point,
						   &normal ) == false )
	{
		return false;
	}
	nbGroundUnderSole( frame, clip, point, normal, lift, tilt );
	return true;
}

// The feet of a standing humanoid follow the ground under the clip: each takes the height and the slope of the ground
// under its sole, smoothly, or the ground of the frame where it finds none. The ground is found again when it is due,
// every eighth update in turn with the other figures, and once the sole moved, not while the figure steps.
static void nbFollowGround( const nbWorld* world, int index, nbFigure* f, const nbClipFoot* clips, bool due, float timeStep,
							float scale )
{
	float follow = b3MinFloat( 1.0f, NB_FOOT_FOLLOW * timeStep );
	b3Vec3 up = b3RotateVector( f->frame.q, b3Vec3_axisY );
	for ( int k = 0; k < 2; ++k )
	{
		nbFoot* foot = f->feet + k;
		const nbClipFoot* clip = clips + k;
		b3Pos sole = b3TransformWorldPoint( f->frame, clip->sole );
		b3Vec3 moved = b3SubPos( sole, foot->castAt );
		moved = b3MulSub( moved, b3Dot( moved, up ), up );
		if ( f->balancing == false &&
			 ( due || foot->cast == false || b3LengthSquared( moved ) > NB_FOOT_RECAST * NB_FOOT_RECAST * scale * scale ) )
		{
			foot->hasGround =
				nbCastFootGround( world, index, sole, up, scale, 0.0f, 0.0f, &foot->groundPoint, &foot->groundNormal );
			foot->castAt = sole;
			foot->cast = true;
		}

		float lift = 0.0f;
		b3Quat tilt = b3Quat_identity;
		if ( foot->hasGround )
		{
			nbGroundUnderSole( f->frame, clip, foot->groundPoint, foot->groundNormal, &lift, &tilt );
		}
		foot->lift += follow * ( lift - foot->lift );
		foot->lean = b3NLerp( foot->lean, tilt, follow );
		foot->down = clip->weight;
	}
}

// A hit that does not knock a standing figure down pushes it, see the top of this section. Returns false where it cannot
// catch the push and falls.
static bool nbPush( nbWorld* world, nbFigure* f, b3Vec3 impulse )
{
	if ( nbCanStep( f ) == false || f->state != nb_figureAnimated )
	{
		return true;
	}

	int index = (int)( f - world->figures.data );
	b3Vec3 up = nbUpOf( world );
	float scale = nbFigureScale( f );
	float gravity = b3Length( b3World_GetGravity( world->physicsWorld ) );
	float omega = sqrtf( b3MaxFloat( gravity, 0.1f ) / b3MaxFloat( f->def.standHeight, 0.1f ) );

	// The velocity the hit gives the body over the ground, on what it moves with, and where it comes to rest
	float mass = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		mass += f->bones[i].mass;
	}
	b3Vec3 across = b3MulSub( impulse, b3Dot( impulse, up ), up );

	// A steered figure carries the push on its own feet: it adds to the velocity of its capsule, see figure_steer.c, as far
	// as the body would come to rest within its reach
	if ( f->steered )
	{
		b3Vec3 change = b3MulSV( 1.0f / mass, across );
		if ( b3Length( change ) > omega * f->def.stepReach )
		{
			return false;
		}
		f->moveVelocity = b3Add( f->moveVelocity, change );
		return true;
	}

	b3Vec3 velocity = b3MulAdd( nbSwayVelocity( f ), 1.0f / mass, across );
	b3Vec3 offset = b3MulSV( 1.0f / omega, velocity );
	float distance = b3Length( offset );

	b3Transform transforms[nb_humanoidBoneCount];
	nbFramePose( f, f->root, f->pose, transforms );
	b3Vec3 center = nbFrameCenter( f, transforms );
	if ( f->balancing == false )
	{
		if ( distance < NB_STEP_MIN * scale )
		{
			return true;
		}
		// Inside the soles shrunk about the center of mass, where the point that far out from it lies inside the soles
		float zone = b3ClampFloat( f->def.stepZone, 0.05f, 1.0f );
		b3Vec3 local = b3InvRotateVector( f->frame.q, offset );
		if ( nbInsideSoles( f, transforms, center.x + local.x / zone, center.z + local.z / zone ) >= 0.0f )
		{
			return true;
		}
	}

	if ( distance > f->def.stepReach || nbBodyBlocked( world, index, f, center, offset, up, scale ) )
	{
		return false;
	}

	// From where the feet stand, as the ground tilts them
	if ( f->balancing == false )
	{
		f->feet[0].plant = f->targets[nb_humanoidFootLeft].p;
		f->feet[1].plant = f->targets[nb_humanoidFootRight].p;
		for ( int k = 0; k < 2; ++k )
		{
			f->feet[k].tilt = nbPartOf( f->feet[k].lean, f->feet[k].down );
		}
		f->swingFoot = -1;
	}
	f->balancing = true;
	f->footWeight = 1.0f;
	f->swayOffset = offset;
	f->swayTime = 0.0f;
	f->swayDuration = 2.0f / omega;
	return true;
}

// The body of a figure that catches a push moves on over the ground
static void nbSway( nbFigure* f, float timeStep )
{
	if ( f->balancing == false || f->swayTime >= f->swayDuration )
	{
		return;
	}
	float before = f->swayTime / f->swayDuration;
	f->swayTime = b3MinFloat( f->swayTime + timeStep, f->swayDuration );
	float after = f->swayTime / f->swayDuration;
	float along = after * ( 2.0f - after ) - before * ( 2.0f - before );
	f->frame.p = b3OffsetPos( f->frame.p, b3MulSV( along, f->swayOffset ) );
}

// The feet of a figure that catches a push: the foot in the air goes on and lands, then the foot that stands farthest from
// where the clip puts it under the body at the end of the push steps there, on the ground there, until both stand there.
// The first step goes with the foot toward the push. Returns false where a step finds no ground and the figure falls.
static bool nbStepFeet( const nbWorld* world, int index, nbFigure* f, const b3Transform* transforms, const nbClipFoot* clips,
						b3Vec3 up, float timeStep )
{
	if ( f->swingFoot >= 0 )
	{
		f->swingTime += timeStep;
		if ( f->swingTime < NB_STEP_TIME )
		{
			return true;
		}
		nbFoot* foot = f->feet + f->swingFoot;
		foot->plant = foot->target;
		foot->tilt = foot->targetTilt;
		f->swingFoot = -1;
		f->stepCount += 1;
	}
	if ( f->balancing == false )
	{
		return true;
	}

	float scale = nbFigureScale( f );
	b3WorldTransform end = { b3OffsetPos( f->frame.p, nbSwayLeft( f ) ), f->frame.q };
	const int feet[2] = { nb_humanoidFootLeft, nb_humanoidFootRight };
	b3Pos places[2];
	float gaps[2];
	for ( int k = 0; k < 2; ++k )
	{
		places[k] = b3TransformWorldPoint( end, transforms[feet[k]].p );
		b3Vec3 gap = b3SubPos( places[k], f->feet[k].plant );
		gaps[k] = b3Length( b3MulSub( gap, b3Dot( gap, up ), up ) );
	}

	int next = gaps[1] > gaps[0] ? 1 : 0;
	if ( fabsf( gaps[1] - gaps[0] ) < NB_STEP_SLACK * scale )
	{
		b3Vec3 local = b3InvRotateVector( f->frame.q, f->swayOffset );
		next = local.x >= 0.0f ? 0 : 1;
	}
	if ( gaps[next] < NB_STEP_SLACK * scale )
	{
		f->balancing = f->swayTime < f->swayDuration;
		return true;
	}

	float lift;
	b3Quat tilt;
	if ( nbFindFootGround( world, index, end, clips + next, scale, 0.0f, 0.0f, &lift, &tilt ) == false )
	{
		return false;
	}
	nbFoot* foot = f->feet + next;
	foot->from = foot->plant;
	foot->fromTilt = foot->tilt;
	foot->target = b3TransformWorldPoint( end, nbTiltedAnkle( clips + next, lift, tilt ) );
	foot->targetTilt = tilt;
	f->swingFoot = next;
	f->swingTime = 0.0f;
	return true;
}

// A swinging foot of a steered humanoid in its gait follows its shin as far as the ankles of people bend. Its toes come
// up against the shin freely to the angle whose sine is NB_SWING_ANKLE_SOFT, 15 degrees, and beyond that less and less,
// never as far as the angle whose sine is NB_SWING_ANKLE_MOST, 25 degrees, the middle of the 20 to 30 degrees people
// reach: the sine it would be beyond the first is over, and it gives over squared by over and the room between the two,
// turned about its ankle, which stays where the leg reaches. A foot that lifts bent farther up keeps how far it is
// beyond as it lifts and gives that up over the first NB_SWING_ANKLE_IN of its swing, and over the last
// NB_SWING_ANKLE_OUT of it the foot comes back to how the gait lands it. Until round 112 the gait held the foot of a
// runner level through its swing while its knee bent to 100 degrees and its shin lay back: the ankle was bent up 65
// degrees at 3.0 m/s and 71 at 4.8 m/s, as no ankle bends, and walking 24 degrees. With a hard stop at 20 degrees that
// let go over the second half of the swing, the toes jumped 36 and 53 mm in an update where it let go. And coming in
// over the first tenth of every swing, instead of keeping what a foot lifts with, it turned a foot down at once that a
// figure starting to run lifted bent up 40 degrees: its toes jumped 47 mm.
#define NB_SWING_ANKLE_SOFT 0.2588f
#define NB_SWING_ANKLE_MOST 0.4226f
#define NB_SWING_ANKLE_IN 0.4f
#define NB_SWING_ANKLE_OUT 0.2f

// A leg of the humanoid reaches for an ankle point in the frame, the knee ahead, the foot at a rotation in the frame:
// the rotations of thigh, shin and foot against their rest, from the pelvis where it is. A foot that swings, along from
// 0 to 1 of its swing and negative for any other, bends its ankle up softly, see NB_SWING_ANKLE_SOFT, and held keeps
// how far it was beyond as it lifted, negative until then.
static void nbReachAnkle( const nbFigure* f, int thigh, b3Transform pelvis, b3Vec3 ankle, b3Quat footRotation, b3Quat* pose,
						  float along, float* held )
{
	const nbBone* thighBone = f->bones + thigh;
	const nbBone* shinBone = f->bones + thigh + 1;
	const nbBone* footBone = f->bones + thigh + 2;
	b3Vec3 hip = b3TransformPoint( pelvis, thighBone->rest.p );
	b3Vec3 knee, end;
	b3Vec3 axis = nbSolveTwoBones( hip, ankle, b3Length( shinBone->rest.p ), b3Length( footBone->rest.p ), b3Vec3_axisZ,
								   &knee, &end );
	b3Quat thighRotation = nbRotationFromAxes( axis, b3Sub( knee, hip ) );
	b3Quat shinRotation = nbRotationFromAxes( axis, b3Sub( end, knee ) );
	if ( along >= 0.0f )
	{
		b3Vec3 shin = b3Normalize( b3Sub( knee, end ) );
		b3Vec3 toes = b3RotateVector( footRotation, b3Vec3_axisY );
		float up = b3Dot( toes, shin );
		float over = b3MaxFloat( up - NB_SWING_ANKLE_SOFT, 0.0f );
		float beyond = over * over / ( NB_SWING_ANKLE_MOST - NB_SWING_ANKLE_SOFT + over );
		if ( *held < 0.0f )
		{
			*held = beyond;
		}
		float in = b3MinFloat( along / NB_SWING_ANKLE_IN, 1.0f );
		float out = b3ClampFloat( ( 1.0f - along ) / NB_SWING_ANKLE_OUT, 0.0f, 1.0f );
		float kept = *held * ( 1.0f - in * in * ( 3.0f - 2.0f * in ) );
		float down = b3MaxFloat( beyond - kept, 0.0f ) * out * out * ( 3.0f - 2.0f * out );
		if ( down > 0.0f )
		{
			// The toes go down that far against the shin, across it as they point, or toward the sole where they point along it
			float bent = up - down;
			b3Vec3 across = b3Sub( toes, b3MulSV( up, shin ) );
			float length = b3Length( across );
			across = length > 1.0e-4f ? b3MulSV( 1.0f / length, across ) : b3Neg( b3RotateVector( footRotation, b3Vec3_axisZ ) );
			b3Vec3 want = b3Normalize( b3Add( b3MulSV( bent, shin ), b3MulSV( sqrtf( 1.0f - bent * bent ), across ) ) );
			footRotation = b3NormalizeQuat( b3MulQuat( b3ComputeQuatBetweenUnitVectors( toes, want ), footRotation ) );
		}
	}
	pose[thigh] = b3NormalizeQuat( b3InvMulQuat( b3MulQuat( pelvis.q, thighBone->rest.q ), thighRotation ) );
	pose[thigh + 1] = b3NormalizeQuat( b3InvMulQuat( b3MulQuat( thighRotation, shinBone->rest.q ), shinRotation ) );
	pose[thigh + 2] = b3NormalizeQuat( b3InvMulQuat( b3MulQuat( shinRotation, footBone->rest.q ), footRotation ) );
}

// A foot of the humanoid rolls over its toes, its heel up by an angle, or by a negative angle over its heel, its toes up:
// its ankle and its rotation in the frame. The toes lie along +y of the foot bone and the sole along -z, see
// nbMakeHumanoidBones, and it rolls about the edge of the toes or of the heel on the sole. A foot in the air rocks about a
// point between them that moves from the one to the other while it is rolled less than rock in radians either way, see
// NB_FOOT_ROCK, so its ankle does not jump in its way as the roll turns from heel to toes.
static void nbRollFoot( const nbBone* footBone, float roll, float rock, b3Vec3* ankle, b3Quat* rotation )
{
	b3Vec3 h = footBone->halfExtents;
	float toward = roll >= 0.0f ? 1.0f : 0.0f;
	if ( rock > 0.0f )
	{
		toward = b3ClampFloat( 0.5f + 0.5f * roll / rock, 0.0f, 1.0f );
		toward = toward * toward * ( 3.0f - 2.0f * toward );
	}
	float along = footBone->center.y + ( 2.0f * toward - 1.0f ) * h.y;
	b3Vec3 edge = b3Add( *ankle, b3RotateVector( *rotation, (b3Vec3){ footBone->center.x, along, footBone->center.z - h.z } ) );
	b3Vec3 axis = b3Normalize( b3Cross( b3RotateVector( *rotation, b3Vec3_axisZ ), b3RotateVector( *rotation, b3Vec3_axisY ) ) );
	b3Quat turn = b3MakeQuatFromAxisAngle( axis, roll );
	*ankle = b3Add( edge, b3RotateVector( turn, b3Sub( *ankle, edge ) ) );
	*rotation = b3NormalizeQuat( b3MulQuat( turn, *rotation ) );
}

// How far a foot on the ground rolls over its toes, or with a negative most over its heel, for its leg to reach it from
// the hip, once the hip lies beyond that edge of the ankle, as far as rolling brings the ankle closer and most at most,
// see nbRollFoot
static float nbRollToReach( const nbBone* footBone, b3Vec3 ankle, b3Quat rotation, b3Vec3 hip, float reach, float most )
{
	float start = b3DistanceSquared( ankle, hip );
	b3Vec3 toes = b3RotateVector( rotation, b3Vec3_axisY );
	float along = toes.x * ( hip.x - ankle.x ) + toes.z * ( hip.z - ankle.z );
	if ( start <= reach * reach || ( most > 0.0f ? along : -along ) <= 0.0f )
	{
		return 0.0f;
	}
	b3Vec3 rolled = ankle;
	b3Quat turned = rotation;
	nbRollFoot( footBone, most, 0.0f, &rolled, &turned );
	float end = b3DistanceSquared( rolled, hip );
	if ( end >= start )
	{
		return 0.0f;
	}
	if ( end > reach * reach )
	{
		return most;
	}

	// Beyond the edge the ankle comes closer the farther the foot rolls
	float low = 0.0f;
	float high = most;
	for ( int i = 0; i < 10; ++i )
	{
		float middle = 0.5f * ( low + high );
		rolled = ankle;
		turned = rotation;
		nbRollFoot( footBone, middle, 0.0f, &rolled, &turned );
		if ( b3DistanceSquared( rolled, hip ) > reach * reach )
		{
			low = middle;
		}
		else
		{
			high = middle;
		}
	}
	return high;
}

// As nbRollToReach over the toes, wherever the hip lies along the foot: as far as the leg needs and no farther than
// brings the ankle closest to the hip, which is where the ankle, turning about the edge of the toes on the sole, lies
// toward the hip from that edge, and never farther than NB_FOOT_ROLL_MOST. Farther it would go away again, and just
// short of it the roll gives the leg next to nothing: 1 cm over the last 0.3 rad. Also returns that closest roll. For a
// foot that stays on its toes although the hip passed over it, see NB_GAIT_FLOAT.
static float nbRollOnToes( const nbBone* footBone, b3Vec3 ankle, b3Quat rotation, b3Vec3 hip, float reach, float* closest )
{
	b3Vec3 h = footBone->halfExtents;
	float along = footBone->center.y + h.y;
	b3Vec3 edge = b3Add( ankle, b3RotateVector( rotation, (b3Vec3){ footBone->center.x, along, footBone->center.z - h.z } ) );
	b3Vec3 axis = b3Normalize( b3Cross( b3RotateVector( rotation, b3Vec3_axisZ ), b3RotateVector( rotation, b3Vec3_axisY ) ) );
	b3Vec3 toAnkle = b3Sub( ankle, edge );
	b3Vec3 toHip = b3Sub( hip, edge );
	toAnkle = b3MulSub( toAnkle, b3Dot( toAnkle, axis ), axis );
	toHip = b3MulSub( toHip, b3Dot( toHip, axis ), axis );
	float top =
		b3ClampFloat( b3Atan2( b3Dot( axis, b3Cross( toAnkle, toHip ) ), b3Dot( toAnkle, toHip ) ), 0.0f, NB_FOOT_ROLL_MOST );
	*closest = top;
	float start = b3DistanceSquared( ankle, hip );
	if ( start <= reach * reach || top <= 0.0f )
	{
		return 0.0f;
	}
	b3Vec3 rolled = ankle;
	b3Quat turned = rotation;
	nbRollFoot( footBone, top, 0.0f, &rolled, &turned );
	float end = b3DistanceSquared( rolled, hip );
	if ( end >= start )
	{
		return 0.0f;
	}
	if ( end > reach * reach )
	{
		return top;
	}

	// Up to the closest the ankle comes closer the farther the foot rolls
	float low = 0.0f;
	float high = top;
	for ( int i = 0; i < 10; ++i )
	{
		float middle = 0.5f * ( low + high );
		rolled = ankle;
		turned = rotation;
		nbRollFoot( footBone, middle, 0.0f, &rolled, &turned );
		if ( b3DistanceSquared( rolled, hip ) > reach * reach )
		{
			low = middle;
		}
		else
		{
			high = middle;
		}
	}
	return high;
}

// How far a leg needs the root to go down to reach an ankle from its hip, less than nothing where it reaches
static float nbDropToReach( b3Vec3 ankle, b3Vec3 hip, float reach )
{
	b3Vec3 offset = b3Sub( ankle, hip );
	float across = offset.x * offset.x + offset.z * offset.z;
	float below = across < reach * reach ? sqrtf( reach * reach - across ) : 0.0f;
	return -offset.y - below;
}

static float nbGaitSlope( const nbFigure* f );

// The way the capsule of a steered humanoid goes from now on, in its frame as it is now: its velocity goes straight to the
// one it wants at its acceleration or braking, see nbMoveSteered, up or down the slope its gait walks on, see
// nbGaitSlope, and its heading turns to where it wants to face at its turn rate. Without climb it does not go up: a foot
// beside or ahead of the hip stands on the farthest ground the figure reached, and the capsule climbs no higher than that
// before the hip passes over the foot, as at the top of stairs.
typedef struct nbPath
{
	b3Vec3 velocity;
	b3Vec3 change;
	float changeTime;
	float turn;
	float turnRate;
} nbPath;

static nbPath nbMakePath( const nbFigure* f, bool climb )
{
	const nbFigureSteering* def = &f->def.steering;
	b3Vec3 velocity = b3InvRotateVector( f->frame.q, f->moveVelocity );
	b3Vec3 wish = b3InvRotateVector( f->frame.q, f->wishVelocity );
	velocity.y = 0.0f;
	wish.y = 0.0f;
	float rate = b3LengthSquared( wish ) > b3LengthSquared( velocity ) ? def->acceleration : def->braking;
	bool facing = b3LengthSquared( wish ) > NB_STEER_FACE_SPEED * NB_STEER_FACE_SPEED;
	float slope = climb ? nbGaitSlope( f ) : b3MinFloat( nbGaitSlope( f ), 0.0f );
	velocity.y = slope * b3Length( velocity );
	wish.y = slope * b3Length( wish );
	nbPath path;
	path.velocity = velocity;
	path.change = b3Sub( wish, velocity );
	path.changeTime = rate > 0.0f ? b3Length( path.change ) / rate : 0.0f;
	if ( rate <= 0.0f )
	{
		path.change = b3Vec3_zero;
	}
	path.turn = 0.0f;
	if ( f->steer.faceVelocity == false )
	{
		path.turn = nbWrapAngle( f->steer.heading - f->heading );
	}
	else if ( facing )
	{
		path.turn = b3Atan2( wish.x, wish.z );
	}
	path.turnRate = b3MaxFloat( def->turnRate, 0.0f );
	return path;
}

// Where a point that goes along with the frame is after a time, in the frame as it is now
static b3Vec3 nbAlongPath( const nbPath* path, b3Vec3 point, float time )
{
	float most = path->turnRate * time;
	b3CosSin turned = b3ComputeCosSin( b3ClampFloat( path->turn, -most, most ) );
	b3Vec3 moved = { turned.cosine * point.x + turned.sine * point.z, point.y, turned.cosine * point.z - turned.sine * point.x };
	float changing = b3MinFloat( time, path->changeTime );
	float share = path->changeTime > 0.0f ? changing / path->changeTime : 1.0f;
	moved = b3MulAdd( moved, time, path->velocity );
	return b3MulAdd( moved, share * ( time - 0.5f * changing ), path->change );
}

// How long the leg of a foot on the ground reaches it as the hip goes along the way of the capsule, without the root going
// lower than limit below where it is now while the hip goes away from the foot, FLT_MAX if longer than NB_GAIT_LOOK in
// seconds or if the hip passes over the foot before, looked at every NB_GAIT_LOOK over NB_GAIT_LOOK_STEPS and found to that
// over 2^NB_GAIT_LOOK_HALVINGS. Behind the hip the leg reaches the foot rolled over its toes, see nbLowerSteered. For a
// foot that stays on its toes, see NB_GAIT_FLOAT, the look goes on past where the hip passes over it.
// A foot on the ground beside or ahead of the hip that the hip leaves is lost once its leg would need the root lower than
// NB_GAIT_LOSE below where the gait has it, in meters for the humanoid of 1.8 m, see nbSteerFeet. It lifts once the other
// foot stands, and while the other foot swings, that one hurries to land before then, up to NB_GAIT_HURRY times as fast.
#define NB_GAIT_LOOK 0.5f
#define NB_GAIT_LOOK_STEPS 10
#define NB_GAIT_LOOK_HALVINGS 5
#define NB_GAIT_LOSE 0.04f
#define NB_GAIT_HURRY 2.5f

// A swing that hurries goes faster by no more than NB_GAIT_HURRY_RATE times its gait per second, and its foot speeds up and
// slows down no harder than NB_GAIT_SWING_ACCEL in meters per second squared on its way from where it lifted to where it
// lands. Walking, the push over the toes comes in over the first NB_GAIT_PUSH_IN in meters for the humanoid of 1.8 m that
// the hip passes the foot.
#define NB_GAIT_HURRY_RATE 10.0f
#define NB_GAIT_SWING_ACCEL 120.0f
#define NB_GAIT_PUSH_IN 0.1f

// The push comes in over the way the hip goes past the foot from where its heel came down at no more than
// NB_GAIT_PUSH_SPEED in meters per second for the humanoid of 1.8 m: where the figure turns round, the hip goes back
// past a foot while it comes down from its heel, and the push came in at once as the heel was down, with a jolt of the knee
#define NB_GAIT_PUSH_SPEED 3.0f

// Going backward, a foot on the ground that its leg only reaches rolled over its toes stays on its toes while the hip
// leaves it, and on once the hip has passed over it lengthwise: it stands far to the side of the hip where the legs
// changed from going sideways to going backward. It rolls as far as its leg needs and no farther than brings the ankle
// closest, see nbRollOnToes, and it is lost once its leg would not reach it rolled as far short of the closest as a
// trailing foot lifts short of NB_FOOT_ROLL_MOST, at most by NB_FOOT_ROLL. Until round 111 it had to come flat as the
// hip passed, no faster than NB_FOOT_HEEL_RATE, and to roll over its heel, which gives the leg 1.3 cm: changing from
// walking sideways to walking backward the root went down 2 to 4 cm in an update, the pelvis jumped up to 39 mm and a
// shin up to 70 mm. And lost only once its leg needed the root NB_GAIT_LOSE lower at the roll it had, the swinging foot
// hurried too late: the roll ran to NB_FOOT_ROLL_MOST and the root went down for the rest.
//
// Walking, a foot stands while the other swings. A trailing foot, or one that stays on its toes, that its leg loses in
// the next update lifts all the same where the other foot lands within NB_GAIT_FLOAT seconds, and for that moment both
// feet are off the ground: holding on, it pulled the root down after it for those updates, and the pelvis came back up
// with a jolt as it lifted. A foot that stands flat beside or ahead of the hip does not: its leg is straight, and
// lifting it made a shin jump 55 instead of 32 mm starting to run sideways at 3.6 m/s. Within 0.08 s instead, tried
// without the foot that stays on its toes, the feet crossed at 26 instead of 14 of 504 points of the changes of the
// gait with clips.
#define NB_GAIT_FLOAT 0.05f

// How many times as fast as its gait a swing goes at most, at least once
static float nbMostHurry( const nbFoot* foot, b3Vec3 up, float swingStart, float swingTime, float gaitRate )
{
	b3Vec3 step = b3SubPos( foot->target, foot->from );
	float across = b3Length( b3MulSub( step, b3Dot( step, up ), up ) );
	float time = ( 1.0f - swingStart ) * swingTime / gaitRate;
	return across > 0.0f ? b3MaxFloat( time * sqrtf( NB_GAIT_SWING_ACCEL / ( 6.0f * across ) ), 1.0f ) : NB_GAIT_HURRY;
}

static float nbTimeToLose( const nbPath* path, b3Vec3 ankle, b3Vec3 rolled, b3Vec3 toes, b3Vec3 hip, float reach, float limit,
						   bool onToes )
{
	bool behind = toes.x * ( hip.x - ankle.x ) + toes.z * ( hip.z - ankle.z ) > 0.0f;
	float step = NB_GAIT_LOOK / (float)NB_GAIT_LOOK_STEPS;
	float before = nbDropToReach( rolled, hip, reach );
	for ( int i = 1; i <= NB_GAIT_LOOK_STEPS; ++i )
	{
		float time = step * (float)i;
		b3Vec3 at = nbAlongPath( path, hip, time );
		if ( ( toes.x * ( at.x - ankle.x ) + toes.z * ( at.z - ankle.z ) > 0.0f ) != behind )
		{
			// A foot that stays on its toes stays on them where the hip passes over it, as long as its leg does not
			// reach it flat, see nbLowerSteered
			if ( onToes && nbDropToReach( ankle, at, reach ) > 0.0f )
			{
				behind = behind == false;
			}
			else
			{
				return FLT_MAX;
			}
		}
		float need = nbDropToReach( rolled, at, reach );
		if ( need > limit && need > before )
		{
			if ( before > limit )
			{
				return time - step;
			}
			float low = time - step;
			float high = time;
			for ( int k = 0; k < NB_GAIT_LOOK_HALVINGS; ++k )
			{
				float middle = 0.5f * ( low + high );
				if ( nbDropToReach( rolled, nbAlongPath( path, hip, middle ), reach ) > limit )
				{
					high = middle;
				}
				else
				{
					low = middle;
				}
			}
			return high;
		}
		before = need;
	}
	return FLT_MAX;
}

// How far the root of a steered humanoid goes down for its legs, and how far its feet roll. The feet on the ground the
// hip did not pass yet lower it as far as their legs need, and it comes up again critically damped, as a knee gives
// way, see NB_ROOT_SETTLE. From there the feet the hip goes away from roll as far as their legs need, behind the hip
// over their toes, running as far as they will need soon, see NB_FOOT_ROLL_AHEAD, and going backward ahead of it over
// their heels, see NB_FOOT_HEEL_MOST, a swinging foot at least as far as it still has the roll it lifted with, see
// nbSteerFeet, and only over the edge it lifted rolled over: one that lifted on its toes rolled over its heel for its
// stretched leg in the update its roll came flat, by up to 19 degrees at once, changing from walking sideways to
// walking backward, and a toe jumped up to 97 mm. Only a foot on the ground lowers the root farther. Going backward, a
// foot on the ground behind the hip, which comes toward it, rolls over its toes as far as its leg needs too, at least
// as far as it still comes down from its toes after it landed on them. A foot that comes down from the edge it landed
// on, see nbSteerFeet, counts as one the hip did not pass yet and keeps the roll the gait gives it, before it lands and
// while it comes down, and a foot rolled over one edge comes flat before it rolls over the other. A swinging foot goes
// where the leg reaches: the gait lowers the root before it lands.
static float nbLowerSteered( nbFigure* f, b3Transform root, const int* thighs, b3Vec3* ankles, b3Quat* rotations,
							 const float* reaches, float planted, float timeStep )
{
	float limit = NB_MAX_DROP * nbFigureScale( f );
	float unroll = nbDecay( timeStep / NB_FOOT_UNROLL );
	nbPath path = nbMakePath( f, true );
	nbPath level = nbMakePath( f, false );
	// Whether the figure goes forward or backward over its feet, see nbLegTurnFor
	float side = f->legBack ? -1.0f : 1.0f;
	bool behind[2];
	bool leads[2];
	bool landed[2];
	bool tips[2];
	float edges[2];
	float drop = 0.0f;
	for ( int k = 0; k < 2; ++k )
	{
		const nbBone* footBone = f->bones + thighs[k] + 2;
		b3Vec3 hip = b3TransformPoint( root, f->bones[thighs[k]].rest.p );
		b3Vec3 toes = b3RotateVector( rotations[k], b3Vec3_axisY );
		float along = toes.x * ( hip.x - ankles[k].x ) + toes.z * ( hip.z - ankles[k].z );
		edges[k] = 0.0f;
		leads[k] = false;
		landed[k] = false;
		tips[k] = false;
		float closest = 0.0f;
		if ( f->footSwings[k] )
		{
			behind[k] = side * along > 0.0f && side * f->footRoll[k] >= 0.0f && side * f->swingRoll[k] >= 0.0f;
		}
		else
		{
			// It rolls over the edge the hip lies beyond: over its toes where the hip is ahead of them, and going backward,
			// over its heel where the hip is behind it. Behind the hip as it goes, it trails. A foot that comes down from the
			// other edge it landed on, or that is rolled over that one, comes flat first, see NB_FOOT_EDGE_FLAT.
			landed[k] = f->landRoll[k] * f->footRoll[k] > 0.0f;
			float edge = along > 0.0f ? 1.0f : ( side < 0.0f && along < 0.0f ? -1.0f : 0.0f );
			if ( edge * f->footRoll[k] < 0.0f && ( landed[k] || fabsf( f->footRoll[k] ) > NB_FOOT_EDGE_FLAT ) )
			{
				edge = 0.0f;
			}
			if ( side < 0.0f )
			{
				// Going backward, a foot rolled over its toes that the hip has passed stays on its toes while its leg needs
				// that, with the roll its leg gave it also while it came down from a landing, and so does one the hip did
				// not pass yet but leaves, as where the legs changed from going sideways, see NB_GAIT_FLOAT. It counts as
				// one the hip did not pass yet.
				bool passed = along <= 0.0f;
				bool leaves = ( hip.x - ankles[k].x ) * level.velocity.x + ( hip.z - ankles[k].z ) * level.velocity.z > 0.0f;
				bool may = passed ? f->footRolled[k] > NB_FOOT_EDGE_FLAT : edge > 0.0f && landed[k] == false && leaves;
				if ( may && planted * nbRollOnToes( footBone, ankles[k], rotations[k], hip, reaches[k], &closest ) > 0.0f )
				{
					tips[k] = true;
					if ( passed )
					{
						edge = 1.0f;
						landed[k] = false;
						f->footRoll[k] = f->footRolled[k];
					}
				}
			}
			edges[k] = edge;
			behind[k] = edge != 0.0f && side * along > 0.0f && tips[k] == false;
			leads[k] = ( edge != 0.0f && side * along < 0.0f ) || tips[k];
		}
		f->footOut[k] = FLT_MAX;
		f->outBehind[k] = behind[k];
		f->outToes[k] = tips[k];
		if ( f->footSwings[k] && f->swingAlong[k] > NB_GAIT_DESCEND )
		{
			// Late in its swing, more and more, as far as its leg needs when the foot lands, from where it lands to the hip
			// where that is then, and a share of what it needs beyond that to reach the foot where it is now, rolled as it
			// lands, on its heel and stepping backward on its toes, see NB_SWING_DROP_SHARE
			float late = b3MinFloat( ( f->swingAlong[k] - NB_GAIT_DESCEND ) / ( 1.0f - NB_GAIT_DESCEND ), 1.0f );
			float stepSide = f->stepBack[k] ? -1.0f : 1.0f;
			b3Vec3 heeled = ankles[k];
			if ( stepSide * f->footRoll[k] < 0.0f )
			{
				b3Quat turned = rotations[k];
				nbRollFoot( footBone, f->footRoll[k], NB_FOOT_ROCK, &heeled, &turned );
			}
			float needNow = nbDropToReach( heeled, hip, reaches[k] );
			float needSwing = needNow;
			if ( needNow > 0.0f )
			{
				b3Vec3 hipThen = nbAlongPath( &path, hip, f->swingRest[k] );
				b3Vec3 targetThen = b3InvRotateVector( f->frame.q, b3SubPos( f->feet[k].target, f->frame.p ) );
				float needThen = b3MaxFloat( nbDropToReach( targetThen, hipThen, reaches[k] ), 0.0f );
				if ( needNow > needThen )
				{
					needSwing = needThen + NB_SWING_DROP_SHARE * ( needNow - needThen );
				}
			}
			drop = b3MaxFloat( drop, late * late * ( 3.0f - 2.0f * late ) * needSwing );
		}
		if ( behind[k] == false && f->footSwings[k] == false )
		{
			// A foot that comes down from the edge it landed on, its ankle where the roll puts it, see nbRollFoot, and going
			// backward one behind the hip, rolled over its toes as far as its leg needs
			b3Vec3 heeled = ankles[k];
			if ( leads[k] )
			{
				float need = tips[k] ? nbRollOnToes( footBone, ankles[k], rotations[k], hip, reaches[k], &closest )
									 : nbRollToReach( footBone, ankles[k], rotations[k], hip, reaches[k], NB_FOOT_ROLL_MOST );
				float roll = b3MaxFloat( landed[k] ? f->footRoll[k] : unroll * f->footRoll[k], planted * need );
				if ( roll != 0.0f )
				{
					b3Quat turned = rotations[k];
					nbRollFoot( footBone, roll, 0.0f, &heeled, &turned );
				}
			}
			else if ( landed[k] )
			{
				b3Quat turned = rotations[k];
				nbRollFoot( footBone, f->footRoll[k], 0.0f, &heeled, &turned );
			}
			else if ( f->footRoll[k] != 0.0f )
			{
				b3Quat turned = rotations[k];
				nbRollFoot( footBone, unroll * f->footRoll[k], 0.0f, &heeled, &turned );
			}
			drop = b3MaxFloat( drop, nbDropToReach( heeled, hip, reaches[k] ) );

			// Beside or ahead of the hip, how long until the leg needs the root lower than NB_GAIT_LOSE below where the gait
			// has it before it goes down for a landing foot
			float lose = NB_GAIT_LOSE * nbFigureScale( f ) - f->gaitCrouch;
			if ( nbGaitClipRooted( f ) )
			{
				// With clips of the gait that move the root, as much less as they crouch, see nbSteerFeet
				lose = NB_GAIT_LOSE * nbFigureScale( f ) - f->gaitClipCrouch;
			}
			f->footOut[k] = nbTimeToLose( &level, ankles[k], heeled, toes, hip, reaches[k], lose, false );
			if ( tips[k] )
			{
				// One that stays on its toes is lost once its leg would not reach it rolled as far short of the closest as
				// a trailing foot lifts short of NB_FOOT_ROLL_MOST, see NB_GAIT_FLOAT
				float lift = b3MinFloat( NB_FOOT_ROLL, closest - ( NB_FOOT_ROLL_MOST - NB_FOOT_ROLL ) );
				b3Vec3 rolled = ankles[k];
				if ( lift > 0.0f )
				{
					b3Quat turned = rotations[k];
					nbRollFoot( footBone, lift, 0.0f, &rolled, &turned );
				}
				float out = nbTimeToLose( &level, ankles[k], rolled, toes, hip, reaches[k], 0.0f, true );
				f->footOut[k] = b3MinFloat( f->footOut[k], out );
			}
		}
	}
	drop = b3ClampFloat( drop, 0.0f, limit );

	// What the gait lowers the root by more than in the last update, see nbSteerFeet, adds to how far it came up again, and
	// what it lowers it by less, the root comes up from as from a landing
	float crouched = b3MinFloat( f->rootCrouch, f->gaitCrouch );
	float above = f->rootDrop - ( crouched + drop );
	float omega = 1.0f / NB_ROOT_SETTLE;
	float decay = nbDecay( omega * timeStep );
	float sum = f->rootDropRate + omega * above;
	float held = decay * ( above + timeStep * sum );
	float rate = decay * ( f->rootDropRate - omega * timeStep * sum );
	if ( held <= 0.0f )
	{
		// It rests where its legs have it, and where they took it down, it keeps a share of the rate it went down with,
		// see NB_ROOT_SETTLE_KEEP
		held = 0.0f;
		rate = above < 0.0f && timeStep > 0.0f ? NB_ROOT_SETTLE_KEEP * -above / timeStep : 0.0f;
	}
	float lowered = b3MinFloat( drop + held, limit );

	b3Transform low = root;
	low.p.y -= lowered;
	float more = 0.0f;
	for ( int k = 0; k < 2; ++k )
	{
		const nbBone* footBone = f->bones + thighs[k] + 2;
		b3Vec3 hip = b3TransformPoint( low, f->bones[thighs[k]].rest.p );
		float most = ( f->footSwings[k] ? side : edges[k] ) < 0.0f ? -NB_FOOT_HEEL_MOST : NB_FOOT_ROLL_MOST;
		float roll = 0.0f;
		if ( tips[k] )
		{
			float closest;
			roll = planted * nbRollOnToes( footBone, ankles[k], rotations[k], hip, reaches[k], &closest );
		}
		else if ( behind[k] || leads[k] )
		{
			roll = planted * nbRollToReach( footBone, ankles[k], rotations[k], hip, reaches[k], most );
		}
		// Running, and as soon as the gait turns to running, before the spring of NB_RUN_TURN_LAG follows, and going backward
		// over the heel also walking, see NB_FOOT_HEEL_MOST
		float rollAhead = side < 0.0f ? 1.0f : b3MaxFloat( f->runTurn, f->gaitRun );
		if ( behind[k] && f->footSwings[k] == false && rollAhead > 0.0f )
		{
			// As far as its leg needs over the next NB_FOOT_ROLL_AHEAD seconds, coming there evenly: the hip goes on
			// along the way of the capsule, turned with the pelvis as nbGaitPose turns it, and up and down with the bob, see
			// nbSteerFeet
			float bobNow = f->gaitBob * b3ComputeCosSin( 4.0f * B3_PI * ( f->gaitPhase - 0.5f * f->gaitStance ) ).cosine;
			b3Vec3 off = b3Sub( hip, low.p );
			bool clipRoot = nbGaitClipRooted( f );
			b3Transform clipNow = clipRoot ? nbGaitClipRoot( f, f->gaitPhase ) : b3Transform_identity;
			float ahead = 0.0f;
			for ( int m = 1;; ++m )
			{
				float time = (float)m * timeStep;
				if ( time >= NB_FOOT_ROLL_AHEAD )
				{
					break;
				}
				float phase = f->gaitPhase + time * f->gaitPhaseRate;
				b3Vec3 then;
				if ( clipRoot )
				{
					// With clips of the gait that move the root, the hip moves and turns as they move the root then
					b3Transform clipThen = nbGaitClipRoot( f, phase - floorf( phase ) );
					b3Quat turn = b3NLerp( b3Quat_identity, b3MulQuat( clipThen.q, b3Conjugate( clipNow.q ) ), f->gaitClipEase );
					b3Vec3 move = b3MulSV( f->gaitClipEase, b3Sub( clipThen.p, clipNow.p ) );
					move.y *= 1.0f - f->stairWeight;
					then = b3Add( b3Add( low.p, move ), b3RotateVector( turn, off ) );
				}
				else
				{
					float turned =
						f->legTurn -
						f->gaitTurnSize * b3ComputeCosSin( 2.0f * B3_PI * ( phase - timeStep * f->gaitPhaseRate ) ).cosine;
					b3CosSin yaw = b3ComputeCosSin( turned - f->gaitTurn );
					float bobThen = f->gaitBob * b3ComputeCosSin( 4.0f * B3_PI * ( phase - 0.5f * f->gaitStance ) ).cosine;
					then = (b3Vec3){ low.p.x + yaw.cosine * off.x + yaw.sine * off.z, hip.y - ( bobThen - bobNow ),
									 low.p.z + yaw.cosine * off.z - yaw.sine * off.x };
				}
				then = nbAlongPath( &path, then, time );
				float soon = 1.0f - time / NB_FOOT_ROLL_AHEAD;
				float need = side * nbRollToReach( footBone, ankles[k], rotations[k], then, reaches[k], most );
				ahead = b3MaxFloat( ahead, soon * soon * ( 3.0f - 2.0f * soon ) * need );
			}
			roll = side * b3MaxFloat( side * roll, planted * rollAhead * ahead );
		}
		if ( f->footSwings[k] )
		{
			roll = side * roll > 0.0f ? side * b3MaxFloat( side * roll, side * f->footRoll[k] ) : f->footRoll[k];
			f->pushKept[k] = 0.0f;
		}
		else
		{
			if ( behind[k] )
			{
				// Trailing, how long until the leg would need the root lower than it goes, rolled over its toes as far as it
				// can, and going backward over its heel by NB_FOOT_HEEL_LIFT, running by less and less: there the hip rises
				// out of the stance faster than the short heel gives the leg, and the leg stretched as it lifted
				b3Vec3 rolled = ankles[k];
				b3Quat turned = rotations[k];
				float heelLift = NB_FOOT_HEEL_LIFT * ( 1.0f - b3MaxFloat( f->runTurn, f->gaitRun ) );
				nbRollFoot( footBone, side > 0.0f ? NB_FOOT_ROLL : -heelLift, 0.0f, &rolled, &turned );
				b3Vec3 toes = b3RotateVector( rotations[k], b3Vec3_axisY );
				f->footOut[k] = nbTimeToLose( &path, ankles[k], rolled, toes, hip, reaches[k], 0.0f, false );
			}
			float edge = edges[k];
			if ( edge * roll <= 0.0f )
			{
				// Beside the hip again, a foot rolled over an edge rolls back flat, see NB_FOOT_UNROLL, and one that comes down
				// from the edge it landed on keeps the roll the gait gives it
				if ( landed[k] )
				{
					roll = f->footRoll[k];
				}
				else if ( behind[k] && edge * f->footRoll[k] >= 0.0f )
				{
					roll = edge * b3MinFloat( edge * f->footRoll[k], 0.0f );
				}
				else
				{
					roll = unroll * f->footRoll[k];
				}
			}
			else if ( landed[k] || leads[k] )
			{
				// As far as its leg needs and at least as far as it comes down from the edge it landed on, or going backward
				// behind the hip, comes back flat over its toes
				roll = edge * b3MaxFloat( edge * roll, edge * ( landed[k] ? f->footRoll[k] : unroll * f->footRoll[k] ) );
			}
			if ( behind[k] == false )
			{
				f->pushPast[k] = 0.0f;
				if ( leads[k] && f->pushKept[k] > 0.0f && roll > 0.0f )
				{
					// It trailed going forward and pushed over its toes, and now the legs walk backward: it loses that push
					// evenly, see NB_FOOT_PUSH_GONE
					f->pushKeptTime[k] += timeStep;
					float gone = f->pushKeptTime[k] / NB_FOOT_UNROLL;
					float kept = ( 1.0f + gone ) * nbDecay( gone );
					roll = b3MinFloat( roll + kept * f->pushKept[k], NB_FOOT_ROLL_MOST );
					if ( kept < NB_FOOT_PUSH_GONE )
					{
						f->pushKept[k] = 0.0f;
					}
				}
				else
				{
					f->pushKept[k] = 0.0f;
				}
			}
			else
			{
				// Going forward, the push comes in as the hip passes over the foot, see NB_GAIT_PUSH_IN and NB_GAIT_PUSH_SPEED,
				// also while the foot comes flat from its heel: held back until then, it came in at once, 0.19 rad in an
				// update where the figure turned from walking backward to walking forward
				b3Vec3 toes = b3RotateVector( rotations[k], b3Vec3_axisY );
				float past = side * ( toes.x * ( hip.x - ankles[k].x ) + toes.z * ( hip.z - ankles[k].z ) );
				past = b3MinFloat( past, f->pushPast[k] + NB_GAIT_PUSH_SPEED * nbFigureScale( f ) * timeStep );
				f->pushPast[k] = past;
				f->pushKept[k] = 0.0f;
				if ( side > 0.0f )
				{
					float push = planted * f->footPush[k] * b3MinFloat( past / ( NB_GAIT_PUSH_IN * nbFigureScale( f ) ), 1.0f );
					roll += push;
					f->pushKept[k] = push;
					f->pushKeptTime[k] = 0.0f;
				}
			}
			if ( roll < 0.0f || f->footRoll[k] < 0.0f )
			{
				// Over its heel no faster than NB_FOOT_HEEL_RATE
				float step = NB_FOOT_HEEL_RATE * timeStep;
				roll = b3ClampFloat( roll, f->footRoll[k] - step, f->footRoll[k] + step );
			}
			else if ( roll < f->footRoll[k] )
			{
				// and back down from its toes no faster either
				roll = b3MaxFloat( roll, f->footRoll[k] - NB_FOOT_HEEL_RATE * timeStep );
			}
			// The legs going forward, it rolls up over its toes no more than NB_FOOT_ROLL_GAIN faster than in the last update,
			// from the roll it was given then
			if ( side > 0.0f && roll > 0.0f && roll > f->footRolled[k] )
			{
				float gain = b3MaxFloat( f->footRollGain[k], 0.0f ) + NB_FOOT_ROLL_GAIN / nbFigureScale( f ) * timeStep;
				roll = b3MinFloat( roll, b3MaxFloat( f->footRolled[k], 0.0f ) + gain * timeStep );
			}
			f->footRollGain[k] = timeStep > 0.0f ? ( roll - f->footRolled[k] ) / timeStep : 0.0f;
			// A foot that stays on its toes goes on rolling over them as it lifts, although the legs go backward
			float over = tips[k] ? 1.0f : side;
			f->footRollRate[k] = timeStep > 0.0f
									 ? over * b3ClampFloat( over * ( roll - f->footRoll[k] ) / timeStep, 0.0f, NB_FOOT_ROLL_RATE )
									 : 0.0f;
			f->footRoll[k] = roll;
			f->footRolled[k] = roll;
		}
		if ( roll != 0.0f )
		{
			nbRollFoot( footBone, roll, f->footSwings[k] ? NB_FOOT_ROCK : 0.0f, ankles + k, rotations + k );
		}
		if ( behind[k] && f->footSwings[k] == false )
		{
			more = b3MaxFloat( more, nbDropToReach( ankles[k], hip, reaches[k] ) );
		}
	}
	lowered = b3ClampFloat( lowered + more, 0.0f, limit );
	f->rootDrop = f->gaitCrouch + lowered;
	f->rootDropRate = rate;
	f->rootCrouch = f->gaitCrouch;
	return lowered;
}

// As the gait of a steered humanoid starts or ends on stairs, the root goes from following the footholds, see
// nbStairGround, to standing over the lower foot, or back, which can lie apart there. As the gait ends because the
// capsule leaves the ground, see nbSteerFeet, the root goes from the height the gait gave it to standing over the lower
// foot. The root goes on from where it was and comes to where it belongs, the difference fading with a time of
// NB_ROOT_CARRY seconds. Until round 104 it went there at once in the air.
static void nbCarryRoot( nbFigure* f, b3Transform* root, b3Vec3 up, float timeStep )
{
	bool carried = f->stairWeight > 0.0f || f->rootCarry != 0.0f || f->onGround == false;
	if ( f->gaitOn != f->carryGait && carried )
	{
		f->rootCarry += b3Dot( b3SubPos( f->carryFrame, f->frame.p ), up ) + f->carryRoot - root->p.y;
	}
	if ( f->rootCarry != 0.0f )
	{
		f->rootCarry *= nbDecay( timeStep / NB_ROOT_CARRY );
		if ( fabsf( f->rootCarry ) < NB_ROOT_CARRY_END * nbFigureScale( f ) )
		{
			f->rootCarry = 0.0f;
		}
		root->p.y += f->rootCarry;
	}
	f->carryGait = f->gaitOn;
	f->carryFrame = f->frame.p;
	f->carryRoot = root->p.y;
}

// The legs of the humanoid reach for its feet. Standing, a foot that the clip holds down takes the height and the slope of
// the ground under its sole, turned about the middle of the sole, and a foot that the clip lifts keeps its pose. As far as
// the figure steps, a foot stands where it planted, or goes on the way of its step, which starts and ends slowly and
// lifts the foot as high as the lift over the higher end. The feet turn as the clip turns them and the ground tilts
// them. The root goes up or down with the lower foot, and down as far as the legs need to reach, never farther than the
// clip stretches them. A steered figure first lifts the heel of a foot behind it as far as its leg needs, see
// nbRollToReach, and its root comes up again slowly, see NB_ROOT_SETTLE.
static void nbPlaceFeet( nbFigure* f, const b3Transform* transforms, const nbClipFoot* clips, b3Vec3 up, float timeStep )
{
	float scale = nbFigureScale( f );
	const int thighs[2] = { nb_humanoidThighLeft, nb_humanoidThighRight };
	b3Vec3 ankles[2];
	b3Quat rotations[2];
	bool placed[2];
	float rise = FLT_MAX;
	float planted = b3ClampFloat( f->footWeight, 0.0f, 1.0f );

	// On ground as flat and high as the clip has it, the clip keeps the legs, but not on stairs or while the root goes on
	// from where it was, see nbCarryRoot
	bool flat = planted == 0.0f && f->stairWeight == 0.0f && f->rootCarry == 0.0f;
	for ( int k = 0; k < 2 && flat; ++k )
	{
		const nbFoot* foot = f->feet + k;
		flat = clips[k].weight == 0.0f || ( fabsf( foot->lift ) < NB_FOOT_FLAT * scale && 1.0f - fabsf( foot->lean.s ) < 1.0e-6f );
	}
	f->feetFlat = flat;
	if ( flat )
	{
		return;
	}

	for ( int k = 0; k < 2; ++k )
	{
		const nbClipFoot* clip = clips + k;
		const nbFoot* foot = f->feet + k;
		b3Quat tilt = clip->weight >= 1.0f ? foot->lean : nbPartOf( foot->lean, clip->weight );
		b3Vec3 ankle = nbTiltedAnkle( clip, clip->weight * foot->lift, tilt );
		if ( planted > 0.0f )
		{
			b3Pos stand = foot->plant;
			b3Quat standTilt = foot->tilt;
			if ( k == f->swingFoot )
			{
				float u = b3ClampFloat( f->swingTime / NB_STEP_TIME, 0.0f, 1.0f );
				float smooth = u * u * ( 3.0f - 2.0f * u );
				float bump = 4.0f * u * ( 1.0f - u );
				float higher = b3MaxFloat( b3Dot( b3SubPos( foot->target, foot->from ), up ), 0.0f );
				float lift = bump * bump * ( NB_STEP_LIFT * scale + higher );
				stand = b3OffsetPos( b3LerpPosition( foot->from, foot->target, smooth ), b3MulSV( lift, up ) );
				standTilt = b3NLerp( foot->fromTilt, foot->targetTilt, smooth );
			}
			ankle = b3Lerp( ankle, b3InvTransformWorldPoint( f->frame, stand ), planted );
			tilt = b3NLerp( tilt, standTilt, planted );
		}
		ankles[k] = ankle;
		b3Quat rotation = clip->rotation;
		if ( f->steered && planted > 0.0f )
		{
			// A steered figure turns over its feet: a foot keeps the heading it planted with, see nbSteerFeet
			float yaw = planted * nbWrapAngle( f->footHeading[k] - f->heading );
			rotation = b3MulQuat( nbHeadingQuat( yaw ), rotation );
		}
		rotations[k] = b3NormalizeQuat( b3MulQuat( tilt, rotation ) );
		placed[k] = planted > 0.0f || clip->weight > 0.0f;
		if ( placed[k] )
		{
			// A foot that the clip lifts moves the root less, as if it went down further
			float down = b3MaxFloat( clip->weight, planted );
			rise = b3MinFloat( rise, ankle.y - clip->ankle.y + ( 1.0f - down ) * NB_MAX_DROP * scale );
		}
	}
	if ( placed[0] == false && placed[1] == false )
	{
		return;
	}

	// Up or down with the lower foot, then down as far as the farther leg needs. In its gait a steered figure keeps the
	// root over the ground under its capsule, see figure_steer.c, and its legs reach down for the feet.
	b3Transform root = f->root;
	if ( f->steered && f->gaitOn )
	{
		// Walking, the pelvis turns and sways over the feet, see nbGaitPose, or as the clips of its gait have it, see
		// nbGaitClipPose
		rise = 0.0f;
		root.q = b3NormalizeQuat( b3MulQuat( nbHeadingQuat( f->gaitTurn ), root.q ) );
		root.p.x += f->gaitSway;
		if ( nbGaitClipRooted( f ) )
		{
			root.q = b3NormalizeQuat( b3MulQuat( f->gaitRoot.q, root.q ) );
			root.p.x += f->gaitRoot.p.x;
			root.p.z += f->gaitRoot.p.z;
		}
	}
	root.p.y += b3ClampFloat( rise, -NB_MAX_DROP * scale, NB_GROUND_ABOVE * scale );
	float reaches[2];
	for ( int k = 0; k < 2; ++k )
	{
		float length = b3Length( f->bones[thighs[k] + 1].rest.p ) + b3Length( f->bones[thighs[k] + 2].rest.p );
		float stretch = b3Length( b3Sub( transforms[thighs[k] + 2].p, transforms[thighs[k]].p ) );
		float most = f->steered && f->gaitOn ? f->legReach : NB_LEG_REACH;
		reaches[k] = b3MaxFloat( most * length, stretch );
	}
	float lowered;
	if ( f->steered && planted > 0.0f )
	{
		lowered = nbLowerSteered( f, root, thighs, ankles, rotations, reaches, planted, timeStep );
	}
	else
	{
		float drop = 0.0f;
		for ( int k = 0; k < 2; ++k )
		{
			b3Vec3 hip = b3TransformPoint( root, f->bones[thighs[k]].rest.p );
			drop = b3MaxFloat( drop, nbDropToReach( ankles[k], hip, reaches[k] ) );
		}
		lowered = b3ClampFloat( drop, 0.0f, NB_MAX_DROP * scale );
		f->rootDrop = 0.0f;
		f->rootDropRate = 0.0f;
	}
	root.p.y -= lowered;
	if ( f->steered )
	{
		nbCarryRoot( f, &root, up, timeStep );
	}
	f->root = root;

	for ( int k = 0; k < 2; ++k )
	{
		if ( placed[k] )
		{
			// A swinging foot goes where the leg reaches, see NB_LEG_SWING_SLACK, and its ankle bends as far as the ankles of
			// people do, see NB_SWING_ANKLE_SOFT
			float along = -1.0f;
			if ( f->steered && f->gaitOn && f->footSwings[k] )
			{
				along = f->swingAlong[k];
				float length = b3Length( f->bones[thighs[k] + 1].rest.p ) + b3Length( f->bones[thighs[k] + 2].rest.p );
				float most = reaches[k] + NB_LEG_SWING_SLACK * length;
				b3Vec3 hip = b3TransformPoint( root, f->bones[thighs[k]].rest.p );
				b3Vec3 offset = b3Sub( ankles[k], hip );
				float distance = b3Length( offset );
				if ( distance > most )
				{
					ankles[k] = b3MulAdd( hip, most / distance, offset );
				}
			}
			else
			{
				f->ankleHeld[k] = -1.0f;
			}
			nbReachAnkle( f, thighs[k], root, ankles[k], rotations[k], f->pose, along, f->ankleHeld + k );
		}
	}
}

// A steered humanoid comes down from where its clip stands to the stance of its gait, see NB_STAND_RISE: as its gait
// goes on, or while it is in the air, its root goes down to the stand height, by NB_STAND_RISE at most, and its knees
// bend as far, its feet where the clip has them, and it comes up again as it stands. Not under a clip the game plays.
// So the idle clip stands upright as a person does, and the gait walks from the stance it had before round 114: with
// the stand height itself 1.8 cm higher the legs stretched to 0.999 of their length wherever the gait put them, and the
// shins jumped over 37 mm at 439 instead of 105 of 504 points of changes. The legs follow from the root and the legs of
// the pose it comes down from and from how far, and from nothing else: where these are the same to the bit as when it
// last solved them, it takes the legs it solved then. The idle clip holds its legs still, and a figure that walks on
// solves them in half of its updates no more, the others differ in the last bit of a sample, see b3NLerp. Solving them
// in every update cost a steered figure 0.4 to 0.5 microseconds an update in round 114.
static void nbComeDownForGait( nbFigure* f, float timeStep )
{
	float target = f->gaitOn || f->onGround == false ? 1.0f : 0.0f;
	float omega = 2.0f / NB_STAND_RISE_TIME;
	float decay = nbDecay( omega * timeStep );
	float off = f->stanceDown - target;
	float sum = f->stanceDownRate + omega * off;
	f->stanceDown = b3ClampFloat( target + decay * ( off + timeStep * sum ), 0.0f, 1.0f );
	f->stanceDownRate = decay * ( f->stanceDownRate - omega * timeStep * sum );
	float above = b3ClampFloat( f->root.p.y - f->def.standHeight, 0.0f, NB_STAND_RISE * nbFigureScale( f ) );
	float down = f->stanceDown * above;
	if ( down <= 0.0f || f->playing )
	{
		return;
	}

	_Static_assert( sizeof( b3Transform ) == 7 * sizeof( float ) && sizeof( b3Quat ) == 4 * sizeof( float ), "floats in a row" );
	_Static_assert( nb_humanoidFootRight - nb_humanoidThighLeft == 5, "the six bones of the legs in a row" );
	float from[32];
	memcpy( from, &f->root, 7 * sizeof( float ) );
	memcpy( from + 7, f->pose + nb_humanoidThighLeft, 24 * sizeof( float ) );
	from[31] = down;
	if ( memcmp( from, f->downFrom, sizeof( from ) ) == 0 )
	{
		memcpy( f->pose + nb_humanoidThighLeft, f->downLegs, sizeof( f->downLegs ) );
		f->root.p.y -= down;
		return;
	}

	b3Transform transforms[nb_humanoidBoneCount];
	nbFrameLegs( f, f->root, f->pose, transforms );
	b3Transform root = f->root;
	root.p.y -= down;
	const int thighs[2] = { nb_humanoidThighLeft, nb_humanoidThighRight };
	for ( int k = 0; k < 2; ++k )
	{
		float held = -1.0f;
		nbReachAnkle( f, thighs[k], root, transforms[thighs[k] + 2].p, transforms[thighs[k] + 2].q, f->pose, -1.0f, &held );
	}
	f->root = root;
	memcpy( f->downFrom, from, sizeof( from ) );
	memcpy( f->downLegs, f->pose + nb_humanoidThighLeft, sizeof( f->downLegs ) );
}

//----------------------------------------------------------------------------------------------------------------------
// The gait of a steered humanoid, see figure_steer.c

// The cadence of the gait in steps per second, in units of the frequency of a pendulum as long as the legs, sqrt(g / l):
// NB_GAIT_CADENCE_MAX less NB_GAIT_CADENCE_SPAN, the span shrinking by e per NB_GAIT_CADENCE_SPEED of speed in units of
// sqrt(g l), and NB_GAIT_CADENCE_MIN at least, for steps on the spot. For legs of 0.96 m: 1.6 steps per second on the
// spot, 2.1 at 1.5 m/s and 3.0 at 4.5 m/s, steps of 0.7 and 1.5 m, as people walk and run.
#define NB_GAIT_CADENCE_MIN 0.5f
#define NB_GAIT_CADENCE_MAX 1.095f
#define NB_GAIT_CADENCE_SPAN 0.719f
#define NB_GAIT_CADENCE_SPEED 0.978f

// Walking, people take fewer and longer steps than that, which fits how they run: the length of their step over their
// steps per minute, the walk ratio, stays about the same from a stroll to a brisk walk, about 0.0067 m per step a
// minute for a person of 1.8 m (Sekiya and Nagasaki 1998, Murakami and Otaka 2017). The cadence of walking is then
// NB_GAIT_CADENCE_WALK times the square root of the speed, in the units above, 0.864 for that walk ratio and legs of
// 0.96 m: 97, 112, 120 and 135 steps a minute at 1.0, 1.4, 1.6 and 2.0 m/s, steps of 0.62 to 0.89 m, where the model
// alone took 112, 124, 130 and 140, steps of 0.54 to 0.86 m. It goes over to the model as far as the figure runs, see
// NB_GAIT_RUN_FROM, or is steered to run: a figure that starts to run or turns round running steps as often as it runs
// from its first step on, as people do, and one that slows down to a walk or stops takes the steps of walking as its
// speed falls. Taking the steps of walking by its speed alone, a figure starting to run at 4.8 m/s came down from its
// first flight later and faster, its pelvis jumped 10.3 instead of 8.4 mm in an update and its hands 17.6 instead of
// 13.2 mm.
#define NB_GAIT_CADENCE_WALK 0.864f

// Walking, a foot that lifts more than an update before its phase says it is due, as its leg loses it, shows how far
// the cycle of the gait is behind the feet, up to NB_GAIT_KEEP_MOST of a cycle, and the cycle catches up, critically
// damped at a rate of one over NB_GAIT_KEEP seconds, so that the next foot lifts when its phase says, after its push,
// see nbSteerFeet. Running, the cycle keeps its own time, and the feet keep to it: a foot whose leg loses it before its
// phase says swings as much slower and lands when its phase comes round, up to NB_GAIT_KEEP_MOST of a cycle later, so
// the feet go on in step with each other and with the bob, see NB_GAIT_BOB_RUN. Before its phase means the nearer way
// round: a foot that lifts more than half a cycle after its phase is early for its next swing, see nbSteerFeet.
#define NB_GAIT_KEEP 0.25f
#define NB_GAIT_KEEP_MOST 0.3f

// The share of its cycle a foot stands on the ground: walking longer than it swings, with both feet down between the
// steps, running shorter, with both feet in the air between them. People change from walking to running about where the
// Froude number v^2 / (g l) passes 0.5, here over the band of speeds from NB_GAIT_RUN_FROM to NB_GAIT_RUN_TO in units of
// sqrt(g l), Froude numbers of 0.38 to 0.64.
#define NB_GAIT_STANCE_WALK 0.6f
#define NB_GAIT_STANCE_RUN 0.33f
#define NB_GAIT_RUN_FROM 0.62f
#define NB_GAIT_RUN_TO 0.8f

// A foot lands where the hip passes over it this far into its stance, walking and running, so it lands closer ahead of the
// body than it leaves behind it, where it rolls over its toes, see nbRollToReach. Walking, the root goes down before a
// foot lands, from NB_GAIT_DESCEND into its swing on, see NB_GAIT_DESCEND, as far as the leg needs to reach where it lands.
#define NB_GAIT_LAND_WALK 0.4f
#define NB_GAIT_LAND_RUN 0.35f

// Running, the root goes down before a foot lands as far as the leg needs in each update until it lands, from where the
// hip is then, turned with the pelvis, and as high as the bob has the root then, see NB_GAIT_BOB_RUN: the leg reaches
// farthest a few updates before the foot lands, while the hip has not caught up with it. The root goes down for each
// over the NB_RUN_DESCEND seconds before, evenly and just in time, and no lower than the leg needs. The foot comes down
// over the second half of its swing more gently than on its arc and lands at 1 - NB_RUN_LAND_SOFT of the speed, see
// nbSwingLift: a foot that slams down stretches the leg until it lands, and the knee bends back at once.
#define NB_RUN_DESCEND 0.16f
#define NB_RUN_LAND_SOFT 0.3f

// On a slope a foot lands farther into its stance up the slope and closer under the body down it, by NB_GAIT_LAND_SLOPE
// of its stance per unit of rise over run, from NB_GAIT_LAND_LOW to NB_GAIT_LAND_HIGH of it: the front leg reaches up as
// far as the back leg reaches down, for which the feet stand farther ahead than behind by twice the length of the leg
// times the rise over run, and in a stance as long as the leg, steps of the gait walking and running, that is one stance
// per unit. The steps get shorter on a slope, the cadence higher by NB_GAIT_STEEP per unit of rise over run.
#define NB_GAIT_LAND_SLOPE 1.0f
#define NB_GAIT_LAND_LOW 0.1f
#define NB_GAIT_LAND_HIGH 0.8f
#define NB_GAIT_STEEP 0.8f

// Walking, a foot turns its toes up by NB_GAIT_HEEL in radians from NB_GAIT_HEEL_FROM of its swing on, lands on its heel
// and comes down from it over NB_GAIT_HEEL_LOAD of the cycle, flat on the ground about a tenth into the cycle, as people
// walk. Running, it lands flat. Stepping backward, it does the same mirrored, as people walk backward: it turns its toes
// down, lands on its toes and comes down from them, and turns its toes up as it lifts, see NB_GAIT_SWING_PITCH. Landing
// on its heel backward, the leg of the humanoid needed the root 7.6 cm lower walking backward at 1.6 m/s, and 14 cm in
// the first step from standing.
#define NB_GAIT_HEEL 0.25f
#define NB_GAIT_HEEL_FROM 0.5f
#define NB_GAIT_HEEL_LOAD 0.1f

// A foot lands on its heel as far as it lands ahead of the hip, along its toes: in full where it lands
// NB_GAIT_HEEL_AHEAD times as far ahead as the gait leads or farther, less in proportion nearer to the hip, and flat
// where it lands under the hip or behind it, while the legs go forward. In a steady gait a foot lands as far ahead as
// the gait leads, 22 to 37 cm walking, running and going sideways, and the steady gaits run as before. Turning about at
// a run, a foot swings and lands behind the hip as the figure runs off the other way, the hip 9 to 16 cm ahead of its
// ankle all through its swing. Until round 123 it turned its toes up all the same, landed on its heel and came flat
// over six updates, and until it was flat it could not count as behind the hip and roll over its toes, see
// nbLowerSteered: the root went down for its leg, 9 cm in five updates, and came up again once the foot rolled, and the
// pelvis jumped 23.6 mm. Landing flat, the foot is behind the hip at once and rolls as far as its leg needs: 8.3 mm.
// To bring such a foot flat faster once it stands is the wrong place, see round 123: its toes slap down, or a foot that
// stands for one update comes flat in that update. While the legs go backward a foot that lands flat with the hip
// ahead of its toes is not behind the hip but ahead of it as the figure goes, and nbLowerSteered has it come down from
// its toes while the root sinks for it, 30 cm in four updates from running sideways to running backward: there a foot
// lands on its heel as before.
#define NB_GAIT_HEEL_AHEAD 0.5f
#define NB_GAIT_SWING_PITCH 0.35f

// Walking, a foot rolls over its toes by NB_GAIT_PUSH in radians farther than its leg needs over the last
// NB_GAIT_PRESWING of the cycle before it lifts, its heel up and its knee bending, so it leaves the ground with a bent
// knee, as people's do. It keeps that push until it lifts, also where it stands longer than its phase says.
#define NB_GAIT_PUSH 0.35f
#define NB_GAIT_PRESWING 0.2f

// The gait starts when the capsule moves faster than NB_GAIT_START or the figure turns, in meters per second, and ends
// when it moves no faster, the speed of the gait is below NB_GAIT_STOP, and both feet stand where the clip puts them,
// within NB_GAIT_SLACK in meters for the humanoid of 1.8 m and turned less than NB_GAIT_TURN in radians from the heading,
// so the clip takes them back without a visible slide. The speed of the gait follows the speed of the capsule at
// NB_GAIT_FOLLOW per second.
#define NB_GAIT_START 0.02f
#define NB_GAIT_STOP 0.1f
#define NB_GAIT_SLACK 0.01f
#define NB_GAIT_TURN 0.08f
#define NB_GAIT_FOLLOW 6.0f

// A swinging foot lifts this high over the higher end of its step, walking and running, in meters for the humanoid of
// 1.8 m, a short step NB_GAIT_LIFT_LOW and half its length, and the foot goes for where it aims without following the
// capsule in the last part of its swing, from NB_GAIT_AIM of it on. It stops following smoothly over NB_GAIT_AIM_FADE of
// its swing before and after that, keeping more and more of where it aimed, so where it lands goes on as it went and
// comes to rest instead of stopping at once. A swing that hurries passes that part in an update or two, so the foot
// also keeps more and more of where it aimed over the last NB_GAIT_AIM_SETTLE in seconds before it lands, all of it
// from NB_GAIT_AIM_SETTLED on, and comes to rest at where it lands instead of chasing it. The first swing of a figure
// that starts walking backward keeps where it aimed from NB_GAIT_AIM_SETTLE_START before it lands on, see
// NB_GAIT_PLAN_FROM: it hurries more as it goes, so early in it the time it has left is too long and where it aims
// comes back a few centimeters late in it. Keeping it from 0.08 s on, the foot landed at 2.5 m/s and jumped 36.9 mm,
// from 0.12 s on at 2.4 m/s and 35.8 mm. Running, the feet land closer under the body, the gap between them smaller by
// NB_GAIT_NARROW, and walking by NB_GAIT_NARROW_WALK, as people walk. Height and gap stay as they were when the foot
// lifted.
#define NB_GAIT_LIFT_WALK 0.06f
#define NB_GAIT_LIFT_RUN 0.22f
#define NB_GAIT_LIFT_LOW 0.02f

// As where a swinging foot lands goes farther from where it lifted, as where the figure turns round and where it lands
// moves far ahead at once, its swing lifts higher no faster than NB_GAIT_LIFT_RISE in meters per second for the humanoid
// of 1.8 m: its height grew with the length of the step at once, and the ankle jumped 9 cm up in an update
#define NB_GAIT_LIFT_RISE 1.0f
#define NB_GAIT_AIM 0.8f
#define NB_GAIT_AIM_FADE 0.1f
#define NB_GAIT_AIM_SETTLE 0.08f
#define NB_GAIT_AIM_SETTLED 0.027f
#define NB_GAIT_AIM_SETTLE_START 0.12f
#define NB_GAIT_NARROW 0.4f
#define NB_GAIT_NARROW_WALK 0.45f

// A swinging foot aims with the velocity of the capsule as a critically damped spring follows it, NB_GAIT_AIM_LAG in
// seconds its time, and while the figure wants to move, as far ahead as the spring lags behind a velocity that changes
// evenly, so where it lands does not change as suddenly as the capsule starts to speed up, slow down or turn, and not late.
// Stopping, it aims without that lead: its target does not rush back as the capsule brakes, and the body comes to rest
// over where it lands.
#define NB_GAIT_AIM_LAG 0.04f

// As the figure starts to run, the capsule speeds up faster than that spring follows it, and the feet plan with the
// acceleration of the capsule: a swinging foot aims for where the capsule will be when it lands, speeding up until it
// has the velocity the figure is steered to. They plan so once the gait runs more than NB_GAIT_PLAN_FROM of the way, or
// the figure is steered faster than people change from walking to running, see NB_GAIT_RUN_TO, fully NB_GAIT_PLAN_SPAN
// above it, and not on stairs. Walking they plan as before. Steered means as fast as the program asks, before the legs
// of a figure that does not face where it moves cap the velocity it goes for, see nbMoveSteered: that cap lets go fast
// at the end of a turn of the legs, the plan came in within one or two updates, a foot aimed 18 cm farther, and the
// pelvis sank 5.5 cm in an update. How fast the cap lets go, the swings plan with too, see NB_PLAN_LOOK. Walking as
// running, off stairs, a foot leads by no more than the gait would at the speed it aims with, see nbGaitLead. Until
// round 108 only running did: walking, a foot led with the time the slow gait of a start leads, at the velocity the
// figure is steered to, and from standing at 1.6 m/s the first step landed 46.5 instead of 36 cm behind the hip going
// backward and 37 instead of 30 cm ahead of it going forward. Backward the root went 7.7 cm low for it and came up
// again once the hip came over the foot, and the pelvis jumped 18.7 mm in an update. Walking, a figure that starts
// backward plans its first swing with the acceleration of the capsule as well, off stairs, until that swing lands. The
// swing is short and hurries as the foot the figure stands on gets lost, and aiming with the velocity the capsule had,
// the foot chased a place that ran away from 13 to 62 cm behind where it stood in 9 updates as the capsule sped up, and
// landed at 3.2 m/s, foot and shin jumping 47.5 and 50.7 mm in an update. Going forward the first swing is long enough
// to land where the later steps land without it, and with it the first step landed 3 cm farther ahead, the root 1.8
// instead of 0.4 cm deeper than at the third landing.
#define NB_GAIT_PLAN_FROM 0.5f
#define NB_GAIT_PLAN_SPAN 0.1f

// As the capsule turns or slows down, a walking figure bends its knees more, as people do, its legs stretching to
// NB_LEG_REACH_BENT of their length at most: a leg near straight moves its knee far for a little change in its reach, and
// a bent one does not snap as a hurried foot lifts and lands. It bends fully once the velocity the capsule wants differs
// from the one the feet aim with by NB_GAIT_BEND_SPEED across it or against it, in meters per second for the humanoid of
// 1.8 m, not as it speeds up, critically damped at a rate of one over NB_GAIT_BEND_IN seconds, and straightens again at
// one over NB_GAIT_BEND_OUT seconds.
#define NB_GAIT_BEND_SPEED 1.0f
#define NB_GAIT_BEND_IN 0.08f
#define NB_GAIT_BEND_OUT 0.3f

// Once the root goes down for a swinging foot, see NB_GAIT_DESCEND, the foot does not aim lower any more: it follows the
// capsule only where the ground lies no more than NB_GAIT_LEVEL below where it aims, in meters for the humanoid of 1.8 m,
// so down stairs it does not change to a lower step late in its swing, which the root could not go down for in time. A
// foot that aims where it found no ground goes on aiming lower.
#define NB_GAIT_LEVEL 0.03f

// Off stairs a swinging foot lands where it finds ground flat enough, no more than NB_GAIT_FOOT_BAND higher or lower than
// the other foot and no higher over the way of the frame than the figure steps, in meters for the humanoid of 1.8 m: where
// the gait puts it, or else the nearest such place NB_GAIT_FOOT_SHIFT apart along its toes, NB_GAIT_FOOT_SHIFTS times to
// either side, or out to its side NB_GAIT_FOOT_OUT times. Where none is, it lands where it misses the band least, counting
// NB_GAIT_FOOT_NEAR of a meter of the band for every meter from where the gait puts it, as long as it steps no higher than
// the figure. Over rubble a foot took the top of a piece half a meter high while the other stood on the ground, and the
// figure sat down between them or lifted its knee to the chest.
#define NB_GAIT_FOOT_BAND 0.25f
#define NB_GAIT_FOOT_SHIFT 0.08f
#define NB_GAIT_FOOT_SHIFTS 3
#define NB_GAIT_FOOT_OUT 2
#define NB_GAIT_FOOT_NEAR 0.025f

// A steered foot lands with its whole sole on one tread of stairs, see nbFindFoothold. Rays down at the middle of the
// sole and NB_FOOTHOLD_MARGIN beyond both of its ends find an edge under it: ground more than NB_FOOTHOLD_TOLERANCE off
// the plane under the middle. On stairs, or once an edge lies under the sole, rays out to NB_FOOTHOLD_REACH find the
// edges of the tread under the middle, each to a sixteenth of the way by halving it NB_FOOTHOLD_HALVINGS times. Off
// stairs a ray at NB_FOOTHOLD_REACH ahead of the middle of a sole without an edge under it looks for stairs ahead, see
// nbLookPast. In meters for the humanoid of 1.8 m.
#define NB_FOOTHOLD_MARGIN 0.01f
#define NB_FOOTHOLD_TOLERANCE 0.02f
#define NB_FOOTHOLD_REACH 0.45f
#define NB_FOOTHOLD_HALVINGS 4

// Treads from NB_STAIR_DEPTH_LOW to NB_STAIR_DEPTH_HIGH deep with edges from NB_STAIR_RISE_LOW to NB_STAIR_RISE_HIGH
// high are stairs, in meters for the humanoid of 1.8 m. On stairs a step goes as many treads as the gait would step on
// a ramp as steep, rounded to the nearest of their ratios, and changes only once the ratio to it changes by
// NB_STAIR_KEEP, but it climbs no more than NB_STAIR_CLIMB in meters for the humanoid of 1.8 m, and the cadence is as
// much faster or slower. The gait counts as on stairs once a tread was found, and that ends as the feet find no more
// edges, the weight halving with every step. A swinging foot goes up to NB_STAIR_RATE faster or slower, so that it lands
// on its tread where the hip passes over it as far into the stance as the gait puts it, and keeps its tread while it can
// still land there so. Each swing keeps its own pace, and the cycle of the gait that of the swing that lands first: as
// the figure ran onto stairs with both feet in the air, the one about to land slowed down the pace of both, the other
// aimed with it up to 0.9 m farther, onto a tread three lower, could not slow down enough to land there in time, and
// landed 49 cm ahead of the hip, and the pelvis went down 29 cm for the leg to reach it. A foot goes to the
// tread a step goes to from the other foot from up to NB_STAIR_SHIFT treads where the gait puts it, so the steps go as
// many treads wherever the figure comes onto the stairs, see nbFindFoothold. The first tread has one edge up and the other
// down, and the tread beyond its front edge is as deep and steps as high, within one NB_STAIR_MATCH-th: rubble has tops
// of pieces as deep as treads and as high above the ground, and the figure took it for stairs in a third of its steps.
#define NB_STAIR_DEPTH_LOW 0.15f
#define NB_STAIR_DEPTH_HIGH 0.5f
#define NB_STAIR_RISE_LOW 0.04f
#define NB_STAIR_RISE_HIGH 0.35f
#define NB_STAIR_KEEP 0.1f
#define NB_STAIR_RATE 0.4f
#define NB_STAIR_CLIMB 0.5f
#define NB_STAIR_SHIFT 2
#define NB_STAIR_MATCH 3.0f

// The tread under the middle of a sole, along its toes: how far it reaches back and forward from the middle, whether an
// edge ends it there, how much higher the ground lies beyond the edge, how deep it is between its edges, the best
// guess, and how much it rises per meter along the toes
typedef struct nbTread
{
	float back;
	float front;
	bool backEdge;
	bool frontEdge;
	float backRise;
	float frontRise;
	float depth;
	float grade;
} nbTread;

// How far the ground lies over a point on a plane, along up, from a ray down from above it: false where there is none,
// which counts as far below
static bool nbGroundOver( const nbWorld* world, int index, b3Pos point, b3Vec3 up, float scale, float* over )
{
	nbFigureQuery ground = nbFindGround( world, index, point, up, NB_GROUND_ABOVE * scale, NB_GROUND_BELOW * scale );
	if ( ground.hit == false )
	{
		return false;
	}
	*over = b3Dot( b3SubPos( ground.point, point ), up );
	return true;
}

// The point a distance along the toes from the middle of a sole on the plane of the ground there, which rises by grade per
// meter along them
static b3Pos nbAlongSole( b3Pos middle, b3Vec3 forward, b3Vec3 up, float grade, float distance )
{
	return b3OffsetPos( middle, b3MulAdd( b3MulSV( distance, forward ), distance * grade, up ) );
}

// The edge between a point on the plane under the middle of a sole and one off it, by their distances along the toes, and
// how far the ground lies over the plane at the first point off it: the last distance found on the plane, halving the way
// between them, and the middle of the last way, nearer the edge
static float nbFindEdge( const nbWorld* world, int index, b3Pos middle, b3Vec3 forward, b3Vec3 up, float grade, float on,
						 float off, float offOver, float scale, float* step, float* edge )
{
	float tolerance = NB_FOOTHOLD_TOLERANCE * scale;
	for ( int i = 0; i < NB_FOOTHOLD_HALVINGS; ++i )
	{
		float half = 0.5f * ( on + off );
		float over = -NB_FOOTHOLD_REACH * scale;
		bool found = nbGroundOver( world, index, nbAlongSole( middle, forward, up, grade, half ), up, scale, &over );
		if ( found && fabsf( over ) <= tolerance )
		{
			on = half;
		}
		else
		{
			off = half;
			offOver = over;
		}
	}
	*step = offOver;
	*edge = 0.5f * ( on + off );
	return on;
}

// The tread under the middle of a sole: rays at the ends of the sole, and if asked out to NB_FOOTHOLD_REACH, find its
// edges. Returns false where the ground under the middle has no plane. The rays start as much higher as they may go
// deeper, so a tread up stairs ahead is found while the capsule still runs on level ground.
static bool nbMeasureTread( const nbWorld* world, int index, b3Pos middle, b3Vec3 forward, b3Vec3 up, float need, bool full,
							float guess, float deeper, float scale, nbTread* tread, b3Pos* ground )
{
	float above = NB_GROUND_ABOVE * scale + b3MaxFloat( -guess, 0.0f ) + deeper;
	float below = NB_GROUND_BELOW * scale + b3MaxFloat( guess, 0.0f ) + deeper;
	nbFigureQuery hit = nbFindGround( world, index, middle, up, above, below );
	if ( hit.hit == false || b3Dot( hit.normal, up ) < NB_STEP_FLATNESS )
	{
		return false;
	}
	float grade = -b3Dot( hit.normal, forward ) / b3Dot( hit.normal, up );
	float tolerance = NB_FOOTHOLD_TOLERANCE * scale;
	float reach = NB_FOOTHOLD_REACH * scale;
	*ground = hit.point;
	float extents[2] = { need, need };
	float nearer[2] = { need, need };
	bool edges[2] = { false, false };
	float rises[2] = { 0.0f, 0.0f };
	// An edge where the ground steps off the plane by NB_STAIR_RISE_LOW to NB_STAIR_RISE_HIGH: where it only bends away, a
	// ramp begins, and the foot does not step up or down farther than stairs go
	float rise = NB_STAIR_RISE_LOW * scale;
	float highest = NB_STAIR_RISE_HIGH * scale;
	for ( int side = 0; side < 2; ++side )
	{
		float sign = side == 0 ? -1.0f : 1.0f;
		float over = -reach;
		float step;
		bool found = nbGroundOver( world, index, nbAlongSole( hit.point, forward, up, grade, sign * need ), up, scale, &over );
		if ( found == false || fabsf( over ) > tolerance )
		{
			// Where it only bends away within the sole, the sole tilts over the bend as on a ramp
			float at;
			float edge =
				sign * nbFindEdge( world, index, hit.point, forward, up, grade, 0.0f, sign * need, over, scale, &step, &at );
			if ( fabsf( step ) >= rise && fabsf( step ) <= highest )
			{
				extents[side] = edge;
				nearer[side] = sign * at;
				edges[side] = true;
				rises[side] = step;
			}
			continue;
		}
		if ( full )
		{
			over = -reach;
			found = nbGroundOver( world, index, nbAlongSole( hit.point, forward, up, grade, sign * reach ), up, scale, &over );
			extents[side] = reach;
			nearer[side] = reach;
			if ( found == false || fabsf( over ) > tolerance )
			{
				float at;
				float edge = sign * nbFindEdge( world, index, hit.point, forward, up, grade, sign * need, sign * reach, over,
												scale, &step, &at );
				if ( fabsf( step ) >= rise && fabsf( step ) <= highest )
				{
					extents[side] = edge;
					nearer[side] = sign * at;
					edges[side] = true;
					rises[side] = step;
				}
			}
		}
	}
	*tread = (nbTread){ extents[0], extents[1], edges[0], edges[1], rises[0], rises[1], nearer[0] + nearer[1], grade };
	return true;
}

// How many treads a step goes on stairs for a step of the gait as long as ratio treads, rounded to the nearest of their
// ratios, but no higher than NB_STAIR_CLIMB, see NB_STAIR_KEEP
static int nbStairTreads( float ratio, float rise, float scale )
{
	int treads = 1;
	while ( ratio > sqrtf( (float)( treads * ( treads + 1 ) ) ) && (float)( treads + 1 ) * rise <= NB_STAIR_CLIMB * scale )
	{
		treads += 1;
	}
	return treads;
}

// Stairs go up at one edge of a tread and down at the other, and the first tread found has one beyond it as deep, whose
// edge is as high: a whole tread found under a sole teaches the figure that it walks stairs and how deep their treads
// are, see NB_STAIR_MATCH, and found right now, a step goes as many treads as one as long as behind would on a ramp as
// steep as the stairs, see NB_STAIR_KEEP
static void nbSeeStairs( const nbWorld* world, int index, nbFigure* f, const nbTread* tread, b3Pos ground, b3Vec3 forward,
						 b3Vec3 up, float need, float behind, float scale )
{
	float depth = tread->depth;
	bool whole = tread->backEdge && tread->frontEdge;
	bool steps = whole && tread->backRise * tread->frontRise < 0.0f;
	if ( steps && f->stairWeight == 0.0f )
	{
		nbTread next;
		b3Pos nextGround;
		b3Pos nextMiddle = b3OffsetPos(
			ground, b3MulAdd( b3MulSV( 0.5f * ( tread->front - tread->back ) + depth, forward ), tread->frontRise, up ) );
		steps = nbMeasureTread( world, index, nextMiddle, forward, up, need, true, 0.0f, 0.0f, scale, &next, &nextGround ) &&
				next.backEdge && next.frontEdge && fabsf( next.depth - depth ) < depth / NB_STAIR_MATCH &&
				fabsf( next.frontRise - tread->frontRise ) < fabsf( tread->frontRise ) / NB_STAIR_MATCH;
	}
	if ( steps && depth >= NB_STAIR_DEPTH_LOW * scale && depth <= NB_STAIR_DEPTH_HIGH * scale )
	{
		f->stairDepth = f->stairDepth > 0.0f ? 0.5f * ( f->stairDepth + depth ) : depth;
		f->stairRise = fabsf( tread->frontRise ) <= fabsf( tread->backRise ) ? tread->frontRise : -tread->backRise;
		f->stairWeight = 1.0f;

		if ( f->stairStep == 0 && behind < 0.0f )
		{
			float ratio = -behind * ( 1.0f + NB_GAIT_STEEP * fabsf( f->slope ) ) /
						  ( ( 1.0f + NB_GAIT_STEEP * fabsf( f->stairRise ) / f->stairDepth ) * f->stairDepth );
			f->stairStep = nbStairTreads( ratio, fabsf( f->stairRise ), scale );
		}
	}
}

// The tread beyond the edge ahead of a sole on a level that ends there, with no edge behind it, may be the first of
// stairs, see nbSeeStairs. The figure learned stairs only from a foot with an edge under its sole: a foot that landed
// just short of the edge of a landing went back onto it, a foot whose sole fitted on one tread saw no edge, and the
// next foot, lifting with the gait of level ground, aimed up to 1.7 m ahead and 0.9 m lower, deeper than the ground is
// looked for, and landed up to four treads down and far ahead of the hip. Running down twenty stairs at 3.0 and 4.5 m/s
// from 300 starts 1 cm apart, the pelvis went up and down against the line of the stairs by more than 10 cm from 9, by
// up to 17 cm, and up at 3.0 m/s a foot stood across the edge of the first tread from 7.
static void nbLookPast( const nbWorld* world, int index, nbFigure* f, const nbTread* level, b3Pos levelGround, b3Vec3 forward,
						b3Vec3 up, float need, float behind, float scale )
{
	if ( f->stairWeight > 0.0f || level->frontEdge == false || level->backEdge )
	{
		return;
	}
	float beyond = level->front + 0.5f * NB_STAIR_DEPTH_LOW * scale;
	b3Pos past = b3OffsetPos( levelGround, b3MulAdd( b3MulSV( beyond, forward ), level->frontRise, up ) );
	nbTread first;
	b3Pos firstGround;
	if ( nbMeasureTread( world, index, past, forward, up, need, true, 0.0f, 0.0f, scale, &first, &firstGround ) )
	{
		nbSeeStairs( world, index, f, &first, firstGround, forward, up, need, behind, scale );
	}
}

// How far along the toes the middle of a sole goes to stand on a tread, need from the middle to beyond each end: to the
// middle of the tread, or as little as it needs where the tread goes on beyond the reach, or against the higher edge of a
// tread shorter than the sole
static float nbFitSole( const nbTread* tread, float need )
{
	if ( tread->back + tread->front >= 2.0f * need )
	{
		if ( tread->backEdge && tread->frontEdge )
		{
			return 0.5f * ( tread->front - tread->back );
		}
		return b3ClampFloat( 0.0f, need - tread->back, tread->front - need );
	}
	bool higherFront = tread->frontEdge && tread->frontRise > 0.0f;
	bool higherBack = tread->backEdge && tread->backRise > 0.0f;
	if ( higherFront != higherBack )
	{
		return higherFront ? tread->front - need : need - tread->back;
	}
	return 0.5f * ( tread->front - tread->back );
}

// Where a swinging foot of a steered humanoid lands with its whole sole on one tread, see NB_FOOTHOLD_MARGIN: how far
// along its toes from where the gait puts the middle of the sole. Where no edge lies under the sole it stays, and false
// is returned. Over an edge it goes onto the tread under the middle, and on stairs onto the tread as many treads beyond
// the other foot as a step goes, see NB_STAIR_KEEP, if that is no more than NB_STAIR_SHIFT treads away. A whole tread
// teaches the figure that it walks stairs and how deep their treads are, see nbSeeStairs, and so does the first tread
// of stairs ahead of a sole on a level, see nbLookPast. A top shorter than the sole with the ground lower beyond both
// of its ends, the peak of a piece of rubble, the foot does not stand on: it goes beyond the nearer end, onto the lower
// ground.
static bool nbFindFoothold( const nbWorld* world, int index, nbFigure* f, int k, b3Pos middle, b3Vec3 forward, b3Vec3 up,
							float guess, float deeper, float scale, float* shift )
{
	const nbBone* footBone = f->bones + ( k == 0 ? nb_humanoidFootLeft : nb_humanoidFootRight );
	float need = footBone->halfExtents.y + NB_FOOTHOLD_MARGIN * scale;
	bool stairs = f->stairWeight > 0.0f;
	nbTread tread;
	b3Pos ground;
	if ( nbMeasureTread( world, index, middle, forward, up, need, stairs, guess, deeper, scale, &tread, &ground ) == false )
	{
		return false;
	}
	if ( stairs == false )
	{
		if ( tread.backEdge == false && tread.frontEdge == false )
		{
			// Where the ground leaves the plane of the sole ahead, the tread it lands on may be one of stairs, or the level
			// end at the first of them, see nbLookPast
			float over = 0.0f;
			bool plane = nbGroundOver( world, index, nbAlongSole( ground, forward, up, tread.grade, NB_FOOTHOLD_REACH * scale ),
									   up, scale, &over ) &&
						 fabsf( over ) <= NB_FOOTHOLD_TOLERANCE * scale;
			nbTread level;
			b3Pos levelGround;
			if ( plane == false &&
				 nbMeasureTread( world, index, middle, forward, up, need, true, guess, deeper, scale, &level, &levelGround ) )
			{
				float behind = b3Dot( b3SubPos( f->foothold[1 - k], middle ), forward );
				nbSeeStairs( world, index, f, &level, levelGround, forward, up, need, behind, scale );
				nbLookPast( world, index, f, &level, levelGround, forward, up, need, behind, scale );
			}
			return false;
		}
		nbMeasureTread( world, index, middle, forward, up, need, true, guess, deeper, scale, &tread, &ground );
	}
	if ( tread.backEdge == false && tread.frontEdge == false )
	{
		return false;
	}

	bool whole = tread.backEdge && tread.frontEdge;
	float behind = b3Dot( b3SubPos( f->foothold[1 - k], middle ), forward );
	if ( whole && tread.backRise < 0.0f && tread.frontRise < 0.0f && tread.back + tread.front < 2.0f * need )
	{
		*shift = tread.front <= tread.back ? tread.front + need : -( tread.back + need );
		return true;
	}

	nbSeeStairs( world, index, f, &tread, ground, forward, up, need, behind, scale );
	nbLookPast( world, index, f, &tread, ground, forward, up, need, behind, scale );
	*shift = nbFitSole( &tread, need );

	// The tread a step of the stairs goes to from the other foot, up to NB_STAIR_SHIFT treads before or after
	if ( whole && f->stairStep > 0 && f->stairDepth > 0.0f )
	{
		float d = f->stairDepth;
		float desired = behind + (float)f->stairStep * d;
		float off = floorf( ( desired - *shift ) / d + 0.5f );
		if ( off != 0.0f && fabsf( off ) <= (float)NB_STAIR_SHIFT )
		{
			nbTread next;
			b3Pos nextGround;
			b3Pos nextMiddle = b3OffsetPos( ground, b3MulAdd( b3MulSV( *shift + off * d, forward ), off * f->stairRise, up ) );
			if ( nbMeasureTread( world, index, nextMiddle, forward, up, need, true, 0.0f, 0.0f, scale, &next, &nextGround ) &&
				 next.backEdge && next.frontEdge && fabsf( next.depth - d ) < 0.3f * d )
			{
				*shift += off * d + nbFitSole( &next, need );
			}
		}
	}
	return true;
}

// The arms swing against the legs by NB_GAIT_ARM radians per unit of speed in sqrt(g l), at most NB_GAIT_ARM_MAX, the
// elbows bend walking and running, the trunk leans forward by NB_GAIT_LEAN radians per unit of speed and the head keeps
// the eyes level, and running the root goes up and down with the steps, in meters for the humanoid of 1.8 m. Up to
// NB_GAIT_EASE in units of speed, they come in with the speed.
#define NB_GAIT_ARM 0.5f
#define NB_GAIT_ARM_MAX 0.8f
#define NB_GAIT_ELBOW_WALK 0.2f
#define NB_GAIT_ELBOW_RUN 1.3f
#define NB_GAIT_LEAN 0.12f
#define NB_GAIT_BOB_RUN 0.035f
#define NB_GAIT_EASE 0.2f

// As the body speeds up, slows down or turns, the trunk leans into it, as a body that falls into what makes it faster:
// the body follows the velocity of the capsule over the ground as a critically damped spring of NB_GAIT_LEAN_LAG seconds,
// and the trunk leans along the acceleration of that spring by atan(a / g), at most NB_GAIT_LEAN_MAX radians, forward as
// it speeds up, back as it brakes and to the inside of a turn, with the head level
#define NB_GAIT_LEAN_LAG 0.3f
#define NB_GAIT_LEAN_MAX 0.5f

// Walking, the pelvis turns by NB_GAIT_PELVIS_TURN in radians to each side, most as a foot lands, the side of the leg in
// front ahead, and sways over the leg it stands on by NB_GAIT_SWAY to each side, in meters for the humanoid of 1.8 m, most
// NB_GAIT_SWAY_AT of the cycle after the foot landed. The chest turns the other way by NB_GAIT_CHEST_TURN with the arms,
// whose elbows bend by NB_GAIT_ELBOW_SWING more as the arm swings forward and less as it swings back, and the head keeps
// facing ahead.
#define NB_GAIT_PELVIS_TURN 0.07f
#define NB_GAIT_CHEST_TURN 0.05f
#define NB_GAIT_SWAY 0.015f
#define NB_GAIT_SWAY_AT 0.3f
#define NB_GAIT_ELBOW_SWING 0.12f

// Running, the chest turns by NB_RUN_CHEST_TURN to each side, against the legs with the shoulders and the arms, and the
// pelvis by NB_RUN_PELVIS_TURN the other way, the side of the leg in front ahead. As the figure speeds up or brakes, the
// speeds between walking and running pass in a few updates, and a critically damped spring of NB_RUN_TURN_LAG seconds
// follows how far it runs. With the spring go the turns, how far the elbows bend and swing, the sway, the bob and where
// in the stance it is lowest, and how high a foot lifts. How low the root runs comes in with the spring and goes with the
// speed, the root rising as from a landing, see nbLowerSteered, the descent of walking for a landing the other way round,
// and the shape of a lift keeps what the foot lifted with, see nbLiftShare. The timing of the steps keeps to the speed:
// walking long on the ground at the speed of running sets the feet 1.5 m apart.
#define NB_RUN_CHEST_TURN 0.21f
#define NB_RUN_PELVIS_TURN 0.09f
#define NB_RUN_TURN_LAG 0.2f

// The slope of the way of a steered humanoid, rise over run: on stairs that of the stairs, along which its body goes, see
// nbStairGround, and elsewhere what its frame went up or down, see figure_steer.c
static float nbGaitSlope( const nbFigure* f )
{
	if ( f->stairWeight > 0.0f && f->stairDepth > 0.0f )
	{
		return f->slope + f->stairWeight * ( f->stairRise / f->stairDepth - f->slope );
	}
	return f->slope;
}

// The share of its cycle a foot of a steered humanoid stands as far as it runs, see NB_GAIT_STANCE_WALK, or with clips
// of its gait as long as in them, blended from the slower to the faster as far as it runs. A single clip walks where
// its feet stand for half the cycle or more and runs otherwise, and the gait of the library takes the other part.
static float nbGaitStance( const nbFigure* f, float run )
{
	if ( f->gaitClipCount == 0 )
	{
		return NB_GAIT_STANCE_WALK + ( NB_GAIT_STANCE_RUN - NB_GAIT_STANCE_WALK ) * run;
	}
	float walking = NB_GAIT_STANCE_WALK;
	float running = NB_GAIT_STANCE_RUN;
	if ( f->gaitClipCount == 1 && f->gaitClips[0].stance < 0.5f )
	{
		running = f->gaitClips[0].stance;
	}
	else
	{
		walking = f->gaitClips[0].stance;
		running = f->gaitClipCount > 1 ? f->gaitClips[1].stance : running;
	}
	return walking + ( running - walking ) * run;
}

// The steps per second of the gait at a speed in units of sqrt(g l), in units of sqrt(g / l), see NB_GAIT_CADENCE_MAX:
// those of walking, see NB_GAIT_CADENCE_WALK, going over to the model as far as the faster of that speed and the speed
// the figure is steered at, wants, is one people run at
static float nbCadenceModel( float u, float wants )
{
	float model =
		b3MaxFloat( NB_GAIT_CADENCE_MAX - NB_GAIT_CADENCE_SPAN * nbDecay( u / NB_GAIT_CADENCE_SPEED ), NB_GAIT_CADENCE_MIN );
	float x = b3ClampFloat( ( b3MaxFloat( u, wants ) - NB_GAIT_RUN_FROM ) / ( NB_GAIT_RUN_TO - NB_GAIT_RUN_FROM ), 0.0f, 1.0f );
	float run = x * x * ( 3.0f - 2.0f * x );
	float walk = b3MaxFloat( NB_GAIT_CADENCE_WALK * sqrtf( b3MaxFloat( u, 0.0f ) ), NB_GAIT_CADENCE_MIN );
	return walk + ( model - walk ) * run;
}

// The steps per second of a steered humanoid at a speed in units of sqrt(g l), omega being sqrt(g / l), see
// NB_GAIT_CADENCE_MAX. With clips of its gait, as many more or fewer as the clips take at the speed they were made at
// than the gait of the library going steadily at that speed, blended from the slower to the faster as far as it runs,
// so it steps as often as the clip at its speed.
static float nbGaitCadence( const nbFigure* f, float omega, float u, float run )
{
	float unit = omega * b3MaxFloat( f->def.standHeight, 0.1f );
	float wants = f->steeredSpeed / unit;
	float cadence = omega * nbCadenceModel( u, wants );
	if ( f->gaitClipCount > 0 )
	{
		float scale[NB_MAX_GAIT_CLIPS];
		for ( int k = 0; k < f->gaitClipCount; ++k )
		{
			const nbGaitClip* gait = f->gaitClips + k;
			float made = gait->speed / unit;
			scale[k] = 2.0f / ( gait->cycle * omega * nbCadenceModel( made, made ) );
		}
		cadence *= f->gaitClipCount > 1 ? scale[0] + ( scale[1] - scale[0] ) * run : scale[0];
	}
	return cadence;
}

// The speed of a steered humanoid as its gait takes it, in units of sqrt(g l) with l its stand height, how far it runs
// rather than walks, from 0 to 1, the share of its cycle a foot stands and the share of its stance where the hip passes
// over it, which the slope moves, see NB_GAIT_LAND_SLOPE
static float nbGaitSpeed( const nbWorld* world, const nbFigure* f, float* run, float* stance, float* land )
{
	float gravity = b3MaxFloat( b3Length( b3World_GetGravity( world->physicsWorld ) ), 0.1f );
	float u = f->gaitSpeed / sqrtf( gravity * b3MaxFloat( f->def.standHeight, 0.1f ) );
	float x = b3ClampFloat( ( u - NB_GAIT_RUN_FROM ) / ( NB_GAIT_RUN_TO - NB_GAIT_RUN_FROM ), 0.0f, 1.0f );
	*run = x * x * ( 3.0f - 2.0f * x );
	*stance = nbGaitStance( f, *run );
	*land = NB_GAIT_LAND_WALK + ( NB_GAIT_LAND_RUN - NB_GAIT_LAND_WALK ) * *run;
	*land = b3ClampFloat( *land + NB_GAIT_LAND_SLOPE * nbGaitSlope( f ), NB_GAIT_LAND_LOW, NB_GAIT_LAND_HIGH );
	return u;
}

// Turn a bone of a pose about an axis of its own
static void nbTurnPoseBone( b3Quat* pose, int bone, b3Vec3 axis, float angle )
{
	pose[bone] = b3NormalizeQuat( b3MulQuat( pose[bone], b3MakeQuatFromAxisAngle( axis, angle ) ) );
}

// Not facing where it moves, see nbSteer::faceVelocity, a steered humanoid turns its legs and its pelvis toward where
// it goes, as people do who look one way and go another: walking as far as NB_LEG_TURN_WALK from its heading and
// running as far as NB_LEG_TURN_RUN, in radians, and going slower than NB_LEG_TURN_EASE in meters per second for the
// humanoid of 1.8 m as much less. The spine turns the chest back by NB_LEG_TURN_SPINE of that, the neck and the head
// the rest, so the head faces the heading. Going farther than NB_LEG_BACK from its heading, the legs walk backward,
// turned toward the heading, until it goes within NB_LEG_FORWARD of it again. They keep where they face in the world as
// the heading turns, turn as fast as the heading may, see nbTurnToward, and as the figure stops, they keep where they
// turned to until its gait goes slower than NB_LEG_TURN_STILL. The pelvis takes them along beyond NB_LEG_TURN_MOST from
// the heading. With legs that faced ahead, the figure stepped to the side across its own feet: at 1.6 m/s the pelvis
// sank 28 cm, at 3.0 m/s the feet crossed by 56 cm. Walking backward, the cadence is NB_GAIT_BACK_CADENCE higher:
// people take shorter steps more often backward than forward at the same speed, about 11 percent more often running
// backward on a treadmill. Running, the flight shortens them, see nbSteerFeet.
#define NB_LEG_TURN_WALK ( 70.0f * ( B3_PI / 180.0f ) )
#define NB_LEG_TURN_RUN ( 90.0f * ( B3_PI / 180.0f ) )
#define NB_LEG_TURN_EASE 0.5f
#define NB_LEG_TURN_SPINE 0.75f
#define NB_LEG_BACK ( 110.0f * ( B3_PI / 180.0f ) )
#define NB_LEG_FORWARD ( 70.0f * ( B3_PI / 180.0f ) )
#define NB_LEG_TURN_STILL 0.05f
#define NB_LEG_TURN_MOST ( 100.0f * ( B3_PI / 180.0f ) )
#define NB_GAIT_BACK_CADENCE 0.11f

// As the legs turn, the feet stay uncrossed across them: the legs turn no farther than the left foot stays NB_LEG_KEEP
// to the left of the right one, in meters for the humanoid of 1.8 m, where the feet are now, on the ground or in the
// air, and wait for that no longer than NB_LEG_HOLD in seconds in a row, counted while the feet leave them less room
// than they turn in an update, then turn on. The foot on the ground on the side they turn to steps out where the feet
// hold them back by more than NB_LEG_BLOCK in radians, whatever its phase, and a swinging foot lands turned as far as
// the legs turn until it lands, at the turn rate of the steering, and on the far side of the turn no farther than the
// feet let the legs turn now. Without all this, the feet crossed by up to 19 cm as the figure stopped going to the side
// and by up to 20 cm as it changed from going left to going right. Until round 114 each foot on the ground stayed
// NB_LEG_KEEP on its own side of the middle instead, a swinging one did not count, and the legs waited 0.15 s, counted
// from when their turn was longer than the room. Turning about as it runs takes the legs 0.7 s, so they did not wait:
// the foot that swung stepped round the standing one against the turn, the foot on the inside of a turn by a quarter
// came to stand on the far side of the other, and the feet crossed by up to 17.6 cm. At 1 512 points of changes of the
// direction the feet cross at 39 instead of 92 now, deeper than 3 cm at 9 instead of 57, 8.7 cm at most. Waiting 0.15,
// 0.3 and 0.5 s they crossed at 61, 48 and 39 of the 480 points of the scenes that cross, waiting 1 s at 29, and with
// the pair on top of the feet on the ground instead of in their place at 39. A foot that only just landed, in the first
// NB_GAIT_FRESH of the cycle, does not lift for the legs: changing from walking sideways to walking backward, it lifted
// again in the update after it landed, the other foot, far behind by then, got lost while it swung, the root went down
// 13 cm and came back as it landed, and the pelvis jumped up to 35 mm in an update.
#define NB_LEG_KEEP 0.06f
#define NB_LEG_HOLD 1.0f
#define NB_LEG_BLOCK ( 10.0f * ( B3_PI / 180.0f ) )

// On top of the pair of feet, since round 116. The legs turn toward where the capsule goes NB_LEG_AIM_AHEAD seconds
// later, as it brakes and speeds up now, and no farther than what it wants: turned toward where it goes now, they began
// to turn about when the capsule had already turned about. A foot in the air that the legs wait for lands up to
// NB_LEG_WAIT_HURRY times as fast where they turn by more than NB_LEG_WAIT_TURN in radians, 100 degrees, and the figure
// does not go backward: its swing stretched to 0.8 s as the gait slowed, and turning about as it runs the capsule
// answered after 1.38 s in the mean and 1.85 s at the latest, now after 1.21 and 1.50 s. Where the body goes across the
// legs toward the side they turn away from, faster than NB_LEG_LEAD_SPEED in m/s for the humanoid of 1.8 m and more
// across than along them, the feet swap their roles: the foot on that side leads the step to the side, so it steps out
// for the legs and lands turned ahead freely, and the other one lands no farther turned than the feet let the legs turn
// now. Changing from walking sideways to walking backward, the foot on the side of the turn stepped out against the way
// of the body, and the pelvis went up to 25 cm across the standing foot. And a foot that lands with the legs turned
// ahead keeps its ankle NB_LAND_LAG_KEEP in meters on its side of the other one also across legs NB_LAND_LAG in radians
// less turned: the pelvis follows the legs about 11 degrees late as they turn, and running from sideways to backward
// the feet of a long step crossed by up to 9 cm across it. At the 1 512 points of round 115 the feet cross at 2 instead
// of 39 and nowhere deeper than 3 cm instead of at 9. Measured at 1 152 points and not built: landing 2.5 times as fast
// for any turn put the feet over their limit at 89 instead of 44 points, the swap without the bound of more across than
// along those of a figure that turns about as it walks at 16 instead of 4, the distance kept between the middles of the
// soles instead of the ankles the hands at 176 instead of 159, and without the legs turned ahead the pelvis jumped up
// to 40.8 mm.
#define NB_LEG_AIM_AHEAD 0.15f
#define NB_LEG_WAIT_HURRY 1.8f
#define NB_LEG_WAIT_TURN 1.75f
#define NB_LEG_LEAD_SPEED 0.1f
#define NB_LAND_LAG 0.2f
#define NB_LAND_LAG_KEEP 0.03f

float nbLegTurnFor( const nbFigure* f, b3Vec3 local, bool* back, float* across )
{
	float speed = sqrtf( local.x * local.x + local.z * local.z );
	float theta = b3Atan2( local.x, local.z );
	return nbLegTurnAt( f, theta, speed, f->legBack, back, across );
}

float nbLegTurnAt( const nbFigure* f, float theta, float speed, bool wasBack, bool* back, float* across )
{
	*back = wasBack ? fabsf( theta ) > NB_LEG_FORWARD : fabsf( theta ) > NB_LEG_BACK;
	float rel = *back ? nbWrapAngle( theta + B3_PI ) : theta;
	// They turn as far as a gait turns that runs as the program asks, where that is more than the run of the gait, which
	// lags, see NB_RUN_TURN_LAG. Starting to run to the side, the capsule ran 4.3 m/s within 0.35 s with the legs turned
	// 72 instead of 90 degrees, and the figure slid across its own feet at 1 m/s, they crossed by up to 10 cm. With the
	// speed of the capsule instead of the one asked for, legs turned for walking capped a capsule asked to run to the
	// side at 3.2 m/s at 1.6 m/s, and at that speed they stayed turned for walking.
	float runs = f->runTurn;
	if ( f->gaitUnit > 0.0f )
	{
		float fast = b3MaxFloat( speed, f->askedSpeed );
		float x = b3ClampFloat( ( fast / f->gaitUnit - NB_GAIT_RUN_FROM ) / ( NB_GAIT_RUN_TO - NB_GAIT_RUN_FROM ), 0.0f, 1.0f );
		runs = b3MaxFloat( runs, x * x * ( 3.0f - 2.0f * x ) );
	}
	float most = NB_LEG_TURN_WALK + runs * ( NB_LEG_TURN_RUN - NB_LEG_TURN_WALK );
	float ease = b3MinFloat( speed / ( NB_LEG_TURN_EASE * nbFigureScale( f ) ), 1.0f );
	float turn = ease * b3ClampFloat( rel, -most, most );
	if ( across != NULL )
	{
		*across = fabsf( b3ComputeCosSin( rel - turn ).sine );
	}
	return turn;
}

// How far a steered humanoid runs follows how far its gait runs as a critically damped spring, see NB_RUN_TURN_LAG
static void nbStepRunTurn( nbFigure* f, float run, float timeStep )
{
	float turnOmega = 1.0f / NB_RUN_TURN_LAG;
	float turnDecay = nbDecay( turnOmega * timeStep );
	float turnOff = f->runTurn - run;
	float turnSum = f->runTurnRate + turnOmega * turnOff;
	f->runTurn = b3ClampFloat( run + turnDecay * ( turnOff + timeStep * turnSum ), 0.0f, 1.0f );
	f->runTurnRate = turnDecay * ( f->runTurnRate - turnOmega * timeStep * turnSum );
}

// The spine of a steered humanoid turns the chest from the pelvis, turned by turn, to counter, and the neck and the
// head turn back by half of counter each, all about up in the frame
static void nbTurnTrunk( nbFigure* f, float turn, float counter )
{
	b3Quat frameQ = f->root.q;
	for ( int i = nb_humanoidBelly; i <= nb_humanoidHead; ++i )
	{
		float angle = i < nb_humanoidBelly + NB_SPINE_COUNT ? ( counter - turn ) / (float)NB_SPINE_COUNT : -0.5f * counter;
		frameQ = b3MulQuat( frameQ, f->bones[i].rest.q );
		b3Quat local = b3NormalizeQuat( b3InvMulQuat( frameQ, b3MulQuat( nbHeadingQuat( angle ), frameQ ) ) );
		f->pose[i] = b3NormalizeQuat( b3MulQuat( local, f->pose[i] ) );
		frameQ = b3MulQuat( frameQ, f->pose[i] );
	}
}

// The body of a steered humanoid in its gait, over the idle clip: the arms swing against the legs, the elbows bend, the
// trunk leans into the speed and into what changes the velocity of the body with the head level, and pelvis and chest turn
// against each other, and with its legs where they turn from the heading, see NB_LEG_TURN_WALK
static void nbGaitPose( const nbWorld* world, nbFigure* f, b3Vec3 up, float timeStep )
{
	f->gaitTurn = 0.0f;
	f->gaitTurnSize = 0.0f;
	f->gaitSway = 0.0f;
	if ( f->boneCount != nb_humanoidBoneCount )
	{
		return;
	}

	// The trunk leans with the acceleration of the body, see NB_GAIT_LEAN_LAG
	b3Vec3 ground = b3MulSub( f->moveVelocity, b3Dot( f->moveVelocity, up ), up );
	b3Vec3 behind = b3Sub( f->leanVelocity, ground );
	nbStepSpring( &behind, &f->leanAcceleration, 1.0f / NB_GAIT_LEAN_LAG, 1.0f, timeStep );
	f->leanVelocity = b3Add( ground, behind );
	b3Vec3 push = b3MulSub( f->leanAcceleration, b3Dot( f->leanAcceleration, up ), up );
	float pushed = b3Length( push );
	if ( pushed > 0.0f )
	{
		float gravity = b3MaxFloat( b3Length( b3World_GetGravity( world->physicsWorld ) ), 0.1f );
		float tilt = b3MinFloat( b3Atan2( pushed, gravity ), NB_GAIT_LEAN_MAX ) / pushed;
		b3Vec3 ahead = b3RotateVector( f->frame.q, b3Vec3_axisZ );
		b3Vec3 left = b3RotateVector( f->frame.q, b3Vec3_axisX );
		float forward = tilt * b3Dot( push, ahead );
		float side = tilt * b3Dot( push, left );
		for ( int i = nb_humanoidBelly; i < nb_humanoidBelly + NB_SPINE_COUNT; ++i )
		{
			nbTurnPoseBone( f->pose, i, b3Vec3_axisX, forward / (float)NB_SPINE_COUNT );
			nbTurnPoseBone( f->pose, i, b3Vec3_axisZ, -side / (float)NB_SPINE_COUNT );
		}
		nbTurnPoseBone( f->pose, nb_humanoidHead, b3Vec3_axisZ, side );
		nbTurnPoseBone( f->pose, nb_humanoidHead, b3Vec3_axisX, -forward );
	}

	// With clips of its gait, they move the rest of the body, see nbGaitClipPose, and the pelvis and the chest turn with
	// the legs as without
	if ( nbGaitClipped( f ) )
	{
		if ( f->legTurn != 0.0f )
		{
			f->gaitTurn = f->legTurn;
			nbTurnTrunk( f, f->legTurn, f->legTurn * ( 1.0f - NB_LEG_TURN_SPINE ) );
		}
		return;
	}
	float run = 0.0f, stance = 0.0f, land = 0.0f;
	float u = f->gaitSpeed > 0.0f ? nbGaitSpeed( world, f, &run, &stance, &land ) : 0.0f;
	nbStepRunTurn( f, run, timeStep );
	if ( f->gaitSpeed <= 0.0f )
	{
		return;
	}
	float ease = b3MinFloat( 1.0f, u / NB_GAIT_EASE );
	ease *= f->gaitBlend;
	b3CosSin cycle = b3ComputeCosSin( 2.0f * B3_PI * f->gaitPhase );
	float swing = b3MinFloat( NB_GAIT_ARM * u, NB_GAIT_ARM_MAX ) * cycle.cosine;
	swing *= f->gaitBlend;
	nbTurnPoseBone( f->pose, nb_humanoidUpperArmLeft, b3Vec3_axisX, -swing );
	nbTurnPoseBone( f->pose, nb_humanoidUpperArmRight, b3Vec3_axisX, swing );
	float running = f->runTurn;
	float walk = ( 1.0f - running ) * ease;
	float elbow = ease * ( NB_GAIT_ELBOW_WALK + ( NB_GAIT_ELBOW_RUN - NB_GAIT_ELBOW_WALK ) * running );
	float elbowSwing = walk * NB_GAIT_ELBOW_SWING * cycle.cosine;
	nbTurnPoseBone( f->pose, nb_humanoidForearmLeft, b3Vec3_axisX, elbow - elbowSwing );
	nbTurnPoseBone( f->pose, nb_humanoidForearmRight, b3Vec3_axisX, elbow + elbowSwing );
	float lean = NB_GAIT_LEAN * u;
	lean *= f->gaitBlend;
	for ( int i = nb_humanoidBelly; i < nb_humanoidBelly + NB_SPINE_COUNT; ++i )
	{
		nbTurnPoseBone( f->pose, i, b3Vec3_axisX, lean / (float)NB_SPINE_COUNT );
	}
	nbTurnPoseBone( f->pose, nb_humanoidHead, b3Vec3_axisX, -lean );

	// The pelvis turns and sways, see nbPlaceFeet, the spine turns the chest the other way, and the neck and the head turn
	// back, each about up in the frame, see NB_GAIT_PELVIS_TURN and NB_RUN_CHEST_TURN. The pelvis turns with the legs, and
	// the chest with them but for NB_LEG_TURN_SPINE of their turn.
	float size = ease * ( NB_GAIT_PELVIS_TURN + running * ( NB_RUN_PELVIS_TURN - NB_GAIT_PELVIS_TURN ) );
	float turn = f->legTurn - size * cycle.cosine;
	float counter = f->legTurn * ( 1.0f - NB_LEG_TURN_SPINE ) +
					ease * ( NB_GAIT_CHEST_TURN + running * ( NB_RUN_CHEST_TURN - NB_GAIT_CHEST_TURN ) ) * cycle.cosine;
	b3CosSin sway = b3ComputeCosSin( 2.0f * B3_PI * ( f->gaitPhase - NB_GAIT_SWAY_AT ) );
	f->gaitTurn = turn;
	f->gaitTurnSize = size;
	f->gaitSway = walk * NB_GAIT_SWAY * nbFigureScale( f ) * sway.cosine;
	nbTurnTrunk( f, turn, counter );
}

// How high a walking foot is over its way, from 0 to 1 of its swing, in parts of its lift: it leaves the ground and comes
// down to it with no speed up or down, is highest about a quarter into its swing, where its toes clear the ground they
// pushed off from, and passes low under the body in the middle of it, as people walk. NB_SWING_EARLY of the lift peaks a
// quarter into the swing and NB_SWING_EVEN of it in the middle.
#define NB_SWING_EARLY 0.75f
#define NB_SWING_EVEN 0.3f

// A swing sets how far it lifts and turns its foot early as it lifts, see NB_GAIT_HURRY, and one that comes to hurry
// more after that lifts less far for it too while its early bump still rises, until NB_SWING_EARLY_TOP of the swing,
// where nbSwingEarly is highest, see nbSteerFeet. The share of the bump it has goes on rising as fast as in the last
// update at most, so the foot does not stop rising at once. Turning about at a run, a foot lifts flat with its leg
// stretched and without hurry, and its swing comes to hurry 1.8 times as much within five updates: with the lift it had
// as it lifted, its ankle rose 15 cm in those five updates, its leg went from 0.98 to 0.80 of its length, and the knee
// of a leg near straight moves twice as far as the leg shortens: a shin jumped 49 mm in an update. At 624 points of
// changes of the gait facing where it moves, at speeds that had not run before, shins jump more than 37 mm at 98
// instead of 146 now, turning about at a run at 25 instead of 65 of 96. Following the hurry at once, the foot stopped
// rising within an update: at 936 points shins jumped at 203 instead of 212 and feet at 64 instead of 54. Never letting
// the share go down, shins at 197, and with the share rising as fast as in the last update, at 150.
#define NB_SWING_EARLY_TOP 0.25f

// A foot that lifts over its toes with its leg stretched to more than NB_SWING_SOFT_FROM of its length, fully from
// NB_SWING_SOFT_TO, lifts gently over the first NB_SWING_SOFT_RAMP of its swing, starting with no speed, acceleration
// or jerk. The walking lift starts with a curvature, and a knee nearly stretched turns several times as fast as the
// ankle moves along the leg: starting sideways at 1.6 m/s, the trailing leg lifted at 0.995 of its length, and its shin
// jumped 38.5 mm in the first update of the swing. A foot that lifts over its heel keeps the fast lift: walking
// backward as the figure turned to look to the side, a shin jumped 53 instead of 39 mm with the gentle lift.
#define NB_SWING_SOFT_FROM 0.98f
#define NB_SWING_SOFT_TO 0.995f
#define NB_SWING_SOFT_RAMP 0.25f

// A bump over a swing, from 0 to 1 of it, that starts and ends with no slope and is 1 a quarter into the swing
static float nbSwingEarly( float u )
{
	float v = 1.0f - u;
	float v3 = v * v * v;
	return u * u * v3 * v3 * ( 65536.0f / 729.0f );
}

// A ramp from 0 to 1 that starts and ends with no slope and no curvature
static float nbSmoothRamp( float x )
{
	return x * x * x * ( 10.0f + x * ( -15.0f + 6.0f * x ) );
}

// How far a swing runs that far along it: as far as the figure ran as the foot lifted, going over in the second half of
// the swing to how far it runs now as the spring of NB_RUN_TURN_LAG has it, so the shape of a lift does not change at
// once as the figure starts or stops running, even where its speed goes through the change from walking to running in a
// few updates, and the foot lands as the gait lands it now
static float nbLiftShare( float lifted, float now, float along )
{
	float w = b3ClampFloat( 2.0f * along - 1.0f, 0.0f, 1.0f );
	return lifted + ( now - lifted ) * w * w * ( 3.0f - 2.0f * w );
}

// How high a swinging foot is over its way that far along its swing: walking early and then evenly, see NB_SWING_EARLY,
// running on an arc that comes down more gently over its second half, see NB_RUN_LAND_SOFT, and lifted running backward
// also goes up as gently over its first half, as a foot ahead of the hip leaves the ground with its leg stretched, and
// over the higher end of a step on an arc
static float nbSwingLift( float along, float run, float height, float higher, float calm, bool back )
{
	float arc = 4.0f * along * ( 1.0f - along );
	float landing = along > 0.5f || back ? arc * ( 1.0f - NB_RUN_LAND_SOFT * ( 1.0f - arc ) ) : arc;
	return arc * higher + run * height * landing +
		   ( 1.0f - run ) * height *
			   ( NB_SWING_EARLY * calm * nbSwingEarly( along ) +
				 NB_SWING_EVEN * 16.0f * along * along * ( 1.0f - along ) * ( 1.0f - along ) );
}

// How much lower than it is the root of a running humanoid has to be for the leg to reach an ankle there, all in the
// frame, from the hip as the pelvis turns it by turned about up through the root, with the root as much higher as the bob
// has it, see NB_RUN_DESCEND
static float nbRunNeed( b3Vec3 root, b3Vec3 hip, b3Vec3 there, float turned, float reach, float high )
{
	b3CosSin yaw = b3ComputeCosSin( turned );
	b3Vec3 off = b3Sub( hip, root );
	float dx = there.x - root.x - yaw.cosine * off.x - yaw.sine * off.z;
	float dz = there.z - root.z - yaw.cosine * off.z + yaw.sine * off.x;
	float wide = b3MinFloat( dx * dx + dz * dz, reach * reach );
	return hip.y - there.y - sqrtf( reach * reach - wide ) - high;
}

// A swinging foot goes to where it aims on a cubic from where it is, as fast as it moves there, to rest where it lands,
// planned anew in every update over the time its swing has left. Where its aim stays, that is the smoothstep from where
// it lifted, and where its aim changes or stops changing, see NB_GAIT_AIM, or the swing hurries, its way bends and its
// velocity does not jump. The move over a step of the time left to an offset from where it is, and its velocity then.
static b3Vec3 nbSwingPath( b3Vec3 offset, b3Vec3* velocity, float left, float step )
{
	if ( left <= step )
	{
		*velocity = b3Vec3_zero;
		return offset;
	}
	b3Vec3 v = *velocity;
	b3Vec3 e = b3MulSub( offset, left, v );
	b3Vec3 c2 = b3MulSV( 1.0f / ( left * left ), b3MulAdd( b3MulSV( 3.0f, e ), left, v ) );
	b3Vec3 c3 = b3MulSV( -1.0f / ( left * left * left ), b3MulAdd( b3MulSV( 2.0f, e ), left, v ) );
	*velocity = b3MulAdd( b3MulAdd( v, 2.0f * step, c2 ), 3.0f * step * step, c3 );
	return b3MulAdd( b3MulAdd( b3MulSV( step, v ), step * step, c2 ), step * step * step, c3 );
}

// The same for an angle, see nbSwingPath
static float nbSwingTurn( float offset, float* rate, float left, float step )
{
	b3Vec3 velocity = { *rate, 0.0f, 0.0f };
	b3Vec3 move = nbSwingPath( (b3Vec3){ offset, 0.0f, 0.0f }, &velocity, left, step );
	*rate = velocity.x;
	return move.x;
}

// Where the clip puts a foot under the frame of a standing humanoid: its ankle on the ground under it, as far as the clip
// holds it down, see nbPlaceFeet
static b3Pos nbClipAnkle( const nbFigure* f, const nbClipFoot* clip, const nbFoot* foot )
{
	b3Quat tilt = clip->weight >= 1.0f ? foot->lean : nbPartOf( foot->lean, clip->weight );
	return b3TransformWorldPoint( f->frame, nbTiltedAnkle( clip, clip->weight * foot->lift, tilt ) );
}

// The ground under the hip of a steered humanoid on stairs, over its frame along up: between the footholds behind and ahead
// of the hip along its way, where the feet stand or land and where they last lifted. The body goes up and down stairs as
// smoothly as on a ramp, where the frame of the capsule goes up or down tread by tread. With footholds on both sides,
// slope is how steeply the ground between them goes up along the way, and otherwise it stays as it is.
static float nbStairGround( const nbFigure* f, b3Vec3 up, b3Vec3 way, b3Pos hip, float* slope )
{
	float behind = 0.0f;
	float ahead = 0.0f;
	float low = 0.0f;
	float high = 0.0f;
	bool back = false;
	bool front = false;
	for ( int i = 0; i < 4; ++i )
	{
		int k = i & 1;
		b3Pos hold = i < 2 ? f->foothold[k] : f->liftHold[k];
		float along = b3Dot( b3SubPos( hold, hip ), way );
		float height = b3Dot( b3SubPos( hold, f->frame.p ), up );
		if ( along <= 0.0f && ( back == false || along > behind ) )
		{
			behind = along;
			low = height;
			back = true;
		}
		else if ( along > 0.0f && ( front == false || along < ahead ) )
		{
			ahead = along;
			high = height;
			front = true;
		}
	}
	if ( back && front )
	{
		*slope = ( high - low ) / ( ahead - behind );
		return low + ( high - low ) * ( -behind / ( ahead - behind ) );
	}
	return back ? low : high;
}

// The feet of a steered humanoid step after its capsule, see figure_steer.c, and its legs reach them in nbPlaceFeet. In
// a cycle of two steps each foot stands for a share of it and swings for the rest, the right foot half a cycle after
// the left. The faster it goes, the more steps per second and the shorter it stands, see NB_GAIT_CADENCE_MAX and
// NB_GAIT_STANCE_RUN. A foot that lifts swings from where it stood to where the hip will pass over it a share of its
// next stance after it lands, see NB_GAIT_LAND_WALK: where the capsule will be when it lands, starting to run as it
// speeds up, see NB_GAIT_PLAN_FROM, and not facing where it moves as its legs turn on and let it go faster, see
// NB_PLAN_LOOK, and on by that share of the stance at the velocity it has then, beside the body
// where the clip puts it, on the ground there. A swing keeps at least the share of the cycle it lifted with. It lifts
// in a smooth arc over the higher end, turns to the heading the figure lands with and loses the roll over its toes it
// lifted with, see nbLowerSteered, its way and its turn planned anew in every update from where it is and how fast it
// moves, see nbSwingPath, and the cycle keeps time with the feet, see NB_GAIT_KEEP. A foot on the ground stands where
// it landed and keeps its heading, so it does not slide, however the figure turns over it. The gait starts as soon as
// the capsule moves or turns and ends when it stands with both feet where the clip puts them. In the air the feet
// follow the clip.

// How much faster than its own a swinging foot goes for the hip to pass over where it lands, with the time it has left at
// its own pace and how far beyond where the gait puts it that lies along a velocity, see NB_STAIR_RATE
static float nbStairPace( float natural, float left, float shift, float along )
{
	return natural / b3MaxFloat( left + shift / along, 0.25f * natural );
}

// How far ahead of the hip the gait lands a foot at a speed, in seconds of that speed, see NB_GAIT_LAND_WALK
static float nbGaitLead( const nbWorld* world, const nbFigure* f, float speed )
{
	float gravity = b3MaxFloat( b3Length( b3World_GetGravity( world->physicsWorld ) ), 0.1f );
	float omega = sqrtf( gravity / b3MaxFloat( f->def.standHeight, 0.1f ) );
	float u = speed / sqrtf( gravity * b3MaxFloat( f->def.standHeight, 0.1f ) );
	float x = b3ClampFloat( ( u - NB_GAIT_RUN_FROM ) / ( NB_GAIT_RUN_TO - NB_GAIT_RUN_FROM ), 0.0f, 1.0f );
	float run = x * x * ( 3.0f - 2.0f * x );
	float stance = nbGaitStance( f, run );
	float land = NB_GAIT_LAND_WALK + ( NB_GAIT_LAND_RUN - NB_GAIT_LAND_WALK ) * run;
	land = b3ClampFloat( land + NB_GAIT_LAND_SLOPE * nbGaitSlope( f ), NB_GAIT_LAND_LOW, NB_GAIT_LAND_HIGH );
	float cadence = nbGaitCadence( f, omega, u, run );
	cadence *= 1.0f + NB_GAIT_STEEP * fabsf( f->slope );
	if ( f->legBack )
	{
		cadence *= 1.0f + ( 1.0f - run ) * NB_GAIT_BACK_CADENCE;
	}
	return land * stance * 2.0f / cadence;
}

// How far the frame goes in a time with a velocity and an acceleration, its velocity changing no farther than to the
// one it is steered to, and its velocity then
static b3Vec3 nbFrameAhead( b3Vec3 velocity, b3Vec3 acceleration, b3Vec3 wish, float time, b3Vec3* then )
{
	float rate = b3Length( acceleration );
	float most = b3Length( b3Sub( wish, velocity ) );
	float tau = rate > 1.0e-3f ? b3MinFloat( time, most / rate ) : 0.0f;
	if ( then != NULL )
	{
		*then = b3MulAdd( velocity, tau, acceleration );
	}
	return b3MulAdd( b3MulSV( time, velocity ), tau * time - 0.5f * tau * tau, acceleration );
}

// As the legs of a humanoid that does not face where it moves turn to where it goes, they let the capsule go faster,
// see nbMoveSteered, most of it at the end of their turn: changing from walking sideways to walking backward at 1.6
// m/s, the capsule went 0.3 to 0.8 m/s for 0.4 s and got to 1.6 m/s in the next 0.1 s. A swinging foot aimed with the
// velocity the capsule had, where it lands ran away from it by up to 3.2 m/s while it was halfway there, the foot on
// the ground got lost, the swing hurried and landed at 2.5 m/s, and foot and shin jumped 51 and 58 mm in an update. The
// swings plan with it: the capsule and its legs are stepped ahead as nbMoveSteered and nbSteerFeet step them, for
// NB_PLAN_LOOK seconds in at most NB_PLAN_STEPS updates, the capsule going for what its legs let it, the legs turning
// to where it goes then as fast as they may, and beside it the capsule with its legs as they are now. Walking, a swing
// adds how much farther the capsule gets as its legs turn on and how much faster it goes then, where it lands and in
// how low the root goes for the landing. Running, where a swing plans with the acceleration of the capsule up to the
// velocity its legs let it go for now, see NB_GAIT_PLAN_FROM, it takes the whole way and the velocity instead, and
// comes back to its own plan over the last NB_PLAN_BLEND of the speed the legs hold back: adding there too, where the
// foot lands moved 4 to 10 cm back and forth from update to update as the legs let go, and the pelvis jumped up to 31
// mm. What it adds does not take a swing across the other foot, across the legs as they are now and still turned to
// where the figure went, unless the swing lands there without it: changing from walking left to walking right the feet
// came 0.8 cm across each other where they stayed 1.9 cm apart before, and from running forward to running sideways 25
// instead of 13 cm. The feet on the ground hold the legs back for a moment, see NB_LEG_KEEP, and the heading may turn:
// stepped ahead as well, the capsule came within 1 mm of where it was said to be instead of within 3 cm, and the feet
// jumped as before. Not on stairs, and with clips of the gait that move the root, not for the first swing from
// standing: the root stays where the clips have it then, for a place farther ahead the swinging leg stretched to 0.996
// of its length, and starting diagonally forward a shin jumped 39 instead of 16 mm.
#define NB_PLAN_LOOK 0.6f
#define NB_PLAN_STEPS 64
#define NB_PLAN_BLEND 0.25f

// The way of the capsule ahead, see NB_PLAN_LOOK, in its frame: how many steps it may look ahead, none while its legs
// do not hold it back, how many it stepped so far, how long a step is and how much of its own plan a running swing
// gives up, the velocity it is asked for and the one its legs let it go for now, then where the stepping is: the
// velocity of the capsule, the one it has with its legs as they are, and how far its legs turned, how fast they turn
// and whether they walk backward, and after each step how far the capsule got and how fast it goes with its legs as
// they are now, and how much farther and faster as they turn on. It steps only as far ahead as a swing asks.
typedef struct nbCapsulePlan
{
	int count;
	int done;
	float step;
	float weight;
	b3Vec3 asked;
	b3Vec3 wishNow;
	float speed;
	float theta;
	float turned[2];
	b3Vec3 going;
	b3Vec3 held;
	float legTurn;
	float legRate;
	bool legBack;
	b3Vec3 way[NB_PLAN_STEPS + 1];
	b3Vec3 velocity[NB_PLAN_STEPS + 1];
	b3Vec3 more[NB_PLAN_STEPS + 1];
	b3Vec3 faster[NB_PLAN_STEPS + 1];
} nbCapsulePlan;

// A velocity over the ground that goes for another one with the acceleration or the braking of the steering, see
// nbMoveSteered
static b3Vec3 nbVelocityToward( b3Vec3 velocity, b3Vec3 wish, const nbFigureSteering* def, float timeStep )
{
	b3Vec3 change = b3Sub( wish, velocity );
	float rate = b3LengthSquared( wish ) > b3LengthSquared( velocity ) ? def->acceleration : def->braking;
	float most = b3MaxFloat( rate, 0.0f ) * timeStep;
	float length = b3Length( change );
	return length <= most ? wish : b3MulAdd( velocity, most / length, change );
}

static void nbPlanCapsule( const nbFigure* f, b3Vec3 up, float timeStep, nbCapsulePlan* plan )
{
	plan->count = 0;
	plan->done = 0;
	if ( f->steer.faceVelocity || f->boneCount != nb_humanoidBoneCount || f->stairWeight > 0.0f || timeStep <= 0.0f )
	{
		return;
	}
	// With clips of its gait that move the root, not for the first swing from standing
	if ( f->gaitStarting && nbGaitClipRooted( f ) )
	{
		return;
	}
	// What the legs let it go for as they are now, after they turned in this update
	b3Vec3 asked = b3InvRotateVector( f->frame.q, b3MulSub( f->askedVelocity, b3Dot( f->askedVelocity, up ), up ) );
	asked.y = 0.0f;
	float speed = sqrtf( asked.x * asked.x + asked.z * asked.z );
	if ( speed <= 0.0f )
	{
		return;
	}
	float theta = b3Atan2( asked.x, asked.z );
	float mostNow = nbLegCap( f, theta, speed, f->legTurn, f->legBack );
	if ( mostNow >= speed )
	{
		return;
	}
	int steps = (int)( NB_PLAN_LOOK / timeStep ) + 1;
	plan->count = steps < NB_PLAN_STEPS ? steps : NB_PLAN_STEPS;
	plan->step = timeStep;
	plan->weight = b3ClampFloat( ( 1.0f - mostNow / speed ) / NB_PLAN_BLEND, 0.0f, 1.0f );
	plan->asked = asked;
	plan->wishNow = b3MulSV( mostNow / speed, asked );
	plan->speed = speed;
	plan->theta = theta;
	bool backward;
	nbLegTurnAt( f, theta, speed, false, &backward, plan->turned );
	nbLegTurnAt( f, theta, speed, true, &backward, plan->turned + 1 );
	plan->going = b3InvRotateVector( f->frame.q, f->moveVelocity );
	plan->going.y = 0.0f;
	plan->held = plan->going;
	plan->legTurn = f->legTurn;
	plan->legRate = f->legHeadingRate;
	plan->legBack = f->legBack;
	plan->way[0] = b3Vec3_zero;
	plan->velocity[0] = plan->held;
	plan->more[0] = b3Vec3_zero;
	plan->faster[0] = b3Vec3_zero;
}

// The way of the capsule ahead stepped on until it has as many steps as asked, or as it may look ahead
static void nbStepPlan( const nbFigure* f, nbCapsulePlan* plan, int steps )
{
	const nbFigureSteering* def = &f->def.steering;
	float timeStep = plan->step;
	float still = NB_LEG_TURN_STILL * nbFigureScale( f );
	for ( int i = plan->done; i < steps && i < plan->count; ++i )
	{
		// The capsule goes for what its legs let it, and beside it for what they let it now
		float turned = plan->turned[plan->legBack ? 1 : 0];
		float most = nbLegCapAcross( f, plan->theta, plan->speed, plan->legTurn, plan->legBack, turned );
		plan->going = nbVelocityToward( plan->going, b3MulSV( most / plan->speed, plan->asked ), def, timeStep );
		plan->held = nbVelocityToward( plan->held, plan->wishNow, def, timeStep );
		b3Vec3 faster = b3Sub( plan->going, plan->held );
		plan->way[i + 1] = b3MulAdd( plan->way[i], timeStep, plan->held );
		plan->velocity[i + 1] = plan->held;
		plan->more[i + 1] = b3MulAdd( plan->more[i], timeStep, faster );
		plan->faster[i + 1] = faster;

		// The legs turn on toward where it goes, see nbSteerFeet
		float going = plan->going.x * plan->going.x + plan->going.z * plan->going.z;
		float legAim = 0.0f;
		if ( going <= still * still )
		{
			plan->legBack = false;
		}
		else
		{
			legAim =
				nbLegTurnAt( f, b3Atan2( plan->going.x, plan->going.z ), sqrtf( going ), plan->legBack, &plan->legBack, NULL );
		}
		float legTurn = nbTurnToward( plan->legTurn, legAim, &plan->legRate, def, timeStep );
		plan->legTurn = b3ClampFloat( legTurn, -NB_LEG_TURN_MOST, NB_LEG_TURN_MOST );
		plan->done = i + 1;
	}
}

// How far the frame goes in a time as a swing plans it, and with then its velocity then, once the legs let the capsule
// go faster as they turn on, see NB_PLAN_LOOK: from the way and the velocity the swing plans without that, walking
// with what the turn adds, and as far as the gait runs, with the whole way of the capsule ahead
static b3Vec3 nbPlanAhead( const nbFigure* f, nbCapsulePlan* plan, float time, float run, b3Vec3 way, b3Vec3* then )
{
	if ( plan->count == 0 )
	{
		return way;
	}
	float at = b3MaxFloat( time, 0.0f ) / plan->step;
	int i = (int)at;
	nbStepPlan( f, plan, i + 1 );
	b3Vec3 more, faster, whole, velocity;
	if ( i >= plan->count )
	{
		// Beyond where it may look, on with the velocities it has there
		int last = plan->count;
		float rest = time - (float)last * plan->step;
		faster = plan->faster[last];
		velocity = b3Add( plan->velocity[last], faster );
		more = b3MulAdd( plan->more[last], rest, faster );
		whole = b3MulAdd( b3Add( plan->way[last], plan->more[last] ), rest, velocity );
	}
	else
	{
		float part = at - (float)i;
		faster = b3Lerp( plan->faster[i], plan->faster[i + 1], part );
		velocity = b3Add( b3Lerp( plan->velocity[i], plan->velocity[i + 1], part ), faster );
		more = b3Lerp( plan->more[i], plan->more[i + 1], part );
		whole = b3Add( b3Lerp( plan->way[i], plan->way[i + 1], part ), more );
	}
	more = b3RotateVector( f->frame.q, more );
	whole = b3RotateVector( f->frame.q, whole );
	if ( then != NULL )
	{
		faster = b3RotateVector( f->frame.q, faster );
		velocity = b3RotateVector( f->frame.q, velocity );
		*then = b3Lerp( b3Add( *then, faster ), b3Lerp( *then, velocity, plan->weight ), run );
	}
	return b3Lerp( b3Add( way, more ), b3Lerp( way, whole, plan->weight ), run );
}

// How long a swing takes: as long as the gait swings now, and no shorter than the share of the cycle it lifted with, so
// a swing that lifted running does not end early as the gait turns to walking or stops and a foot swings for less of
// the cycle, see nbSteerFeet. A swing that lifted while the gait ran less than it does now takes no longer than the
// gait swung when it lifted, or as long as its foot needs for its way at NB_GAIT_SWING_ACCEL. As a figure breaks into a
// run, a foot swings for two thirds of the cycle instead of two fifths within a few updates, while the foot on the
// ground is lost as early as walking: until round 117 a foot in the air stretched its swing from 0.32 to 0.53 s, and
// starting to run and changing direction running both feet were in the air for up to 0.47 s, where a steady run flies
// 0.13 to 0.17 s. At speeds that had not run before, both feet are in the air for more than 0.2 s at 156 instead of 269
// of 1 008 points with a fixed heading and at 50 instead of 118 of 312 facing where it moves. Without the bound of the
// foot a swing that kept its time braked harder as it landed, feet over 37 mm in an update at 83 instead of 54 of 2 520
// points.
static float nbSwingTime( const nbFigure* f, int k, float swingTime, float cycle, b3Vec3 up )
{
	if ( f->footSwings[k] == false )
	{
		return swingTime;
	}
	float time = b3MaxFloat( swingTime, f->swingShare[k] * cycle );
	if ( f->gaitRun > f->swingRun[k] )
	{
		b3Vec3 step = b3SubPos( f->feet[k].target, f->feet[k].from );
		float across = b3Length( b3MulSub( step, b3Dot( step, up ), up ) );
		float may = f->swingRate[k] * f->swingHurry[k] * sqrtf( 6.0f * across / NB_GAIT_SWING_ACCEL );
		time = b3MinFloat( time, b3MaxFloat( f->swingLift[k], may ) );
	}
	return time;
}

// As a steered figure turns, its feet keep its legs from crossing. A planted foot turns on the middle of its sole with the
// body once the heading is more than NB_FOOT_TWIST in radians from it, as people pivot on a foot, so the hip never twists
// further. A swinging foot lands on its own side of the other foot, the middles of the soles at least NB_GAIT_SIDE_GAP
// apart across the heading, in meters for the humanoid of 1.8 m. A foot that lands and gets lost at once, in the first
// NB_GAIT_FRESH of the cycle, stands while the other foot, behind the hip and soon out of reach as well, lifts first: the
// foot on the outside of a turn lands wide, and lifting it again at once left the other foot standing far behind the hip.
#define NB_FOOT_TWIST ( 35.0f * ( B3_PI / 180.0f ) )
#define NB_GAIT_SIDE_GAP 0.1f
#define NB_GAIT_FRESH 0.2f

// Off stairs where a swinging foot lands, see NB_GAIT_FOOT_BAND: at the place end where the gait puts it, whose ground was
// found as lift and tilt, or else at the nearest place along its toes or out to its side with ground within the band of
// the other foot and no higher over the way of the frame than step, or else at the place with ground no higher than step
// that misses the band least. The ground is cast as from the guess with deeper, see nbCastFootGround. False where all the
// ground there is higher than step.
static bool nbChooseFoothold( const nbWorld* world, int index, const nbFigure* f, int k, const nbClipFoot* aim, b3Vec3 forward,
							  b3Vec3 up, float guess, float deeper, float step, float scale, b3WorldTransform* end, float* lift,
							  b3Quat* tilt )
{
	int j = 1 - k;
	b3Pos other = f->footSwings[j] ? f->feet[j].target : f->feet[j].plant;
	b3Vec3 across = b3RotateVector( end->q, b3Vec3_axisX );
	across = b3Normalize( b3MulSub( across, b3Dot( across, up ), up ) );
	float out = k == 0 ? 1.0f : -1.0f;
	float band = NB_GAIT_FOOT_BAND * scale;
	b3WorldTransform start = *end;
	float bestMiss = FLT_MAX;
	bool found = false;
	for ( int ring = 0; ring <= NB_GAIT_FOOT_SHIFTS; ++ring )
	{
		// The place itself, then at each distance back and forward along the toes, and out to the side
		int places = ring == 0 ? 1 : ( ring <= NB_GAIT_FOOT_OUT ? 3 : 2 );
		for ( int i = 0; i < places; ++i )
		{
			float distance = (float)ring * NB_GAIT_FOOT_SHIFT * scale;
			b3WorldTransform place = start;
			float placeLift = *lift;
			b3Quat placeTilt = *tilt;
			if ( ring > 0 )
			{
				b3Vec3 offset = i == 2 ? b3MulSV( out * distance, across ) : b3MulSV( i == 0 ? -distance : distance, forward );
				place.p = b3OffsetPos( start.p, offset );
				if ( nbFindFootGround( world, index, place, aim, scale, guess, deeper, &placeLift, &placeTilt ) == false )
				{
					continue;
				}
			}
			if ( placeLift > step )
			{
				continue;
			}
			float rise =
				b3Dot( b3SubPos( b3TransformWorldPoint( place, nbTiltedAnkle( aim, placeLift, placeTilt ) ), other ), up );
			float miss = b3MaxFloat( fabsf( rise ) - band, 0.0f );
			if ( miss + NB_GAIT_FOOT_NEAR * distance < bestMiss )
			{
				bestMiss = miss + NB_GAIT_FOOT_NEAR * distance;
				*end = place;
				*lift = placeLift;
				*tilt = placeTilt;
				found = true;
			}
			if ( miss == 0.0f )
			{
				return true;
			}
		}
	}
	return found;
}

// Where the legs may turn to on their way from where they face to where they turn to, both in the world, see
// NB_LEG_KEEP: no farther than the feet stay uncrossed across them, until they waited for the feet longer than
// NB_LEG_HOLD. It marks the foot on the ground that steps out for them, see NB_LEG_LEAD_SPEED, and the foot in the air
// they wait for, see NB_LEG_WAIT_HURRY.
static float nbHoldLegs( nbFigure* f, float from, float to, float scale, float timeStep )
{
	f->legBlocks[0] = false;
	f->legBlocks[1] = false;
	f->legRoom = FLT_MAX;
	f->legWaits = -1;
	float turn = nbWrapAngle( to - from );
	f->legLead = turn > 0.0f ? 0 : 1;
	float room = FLT_MAX;
	if ( turn != 0.0f && f->gaitOn )
	{
		// Where the left foot is from the right one across and along the legs: its angle there, from ahead toward the
		// left, stays between low and pi - low, so it stays keep to the left. Feet nearer each other than keep hold no
		// legs.
		b3CosSin legs = b3ComputeCosSin( from );
		b3Vec3 offset = b3SubPos( f->feet[0].plant, f->feet[1].plant );
		float across = offset.x * legs.cosine - offset.z * legs.sine;
		float along = offset.x * legs.sine + offset.z * legs.cosine;
		float apart = sqrtf( across * across + along * along );
		float keep = NB_LEG_KEEP * scale;
		if ( apart > keep )
		{
			float ratio = keep / apart;
			float low = b3Atan2( ratio, sqrtf( b3MaxFloat( 1.0f - ratio * ratio, 0.0f ) ) );
			float angle = b3Atan2( across, along );
			float space;
			if ( low <= angle && angle <= B3_PI - low )
			{
				// Turning left takes the angle down toward low, turning right up toward pi - low
				space = turn > 0.0f ? angle - low : B3_PI - low - angle;
			}
			else
			{
				// Crossed, or less than keep apart across the legs: they turn only the way that brings the feet apart
				bool down = angle > B3_PI - low || angle < -0.5f * B3_PI;
				space = ( turn > 0.0f ) == down ? FLT_MAX : 0.0f;
			}
			room = space;
			// The foot that steps out for the legs: the one on the side they turn to, and where the body goes across them
			// toward the other side, more across than along, the one on that side, which leads the step to the side
			int lead = turn > 0.0f ? 0 : 1;
			float sideways = f->gaitVelocity.x * legs.cosine - f->gaitVelocity.z * legs.sine;
			float toFar = turn > 0.0f ? -sideways : sideways;
			float alongLegs = f->gaitVelocity.x * legs.sine + f->gaitVelocity.z * legs.cosine;
			if ( toFar > NB_LEG_LEAD_SPEED * scale && toFar > fabsf( alongLegs ) )
			{
				lead = 1 - lead;
			}
			f->legLead = lead;
			bool holds = space + NB_LEG_BLOCK < fabsf( turn );
			if ( f->footSwings[1 - lead] && holds && fabsf( turn ) > NB_LEG_WAIT_TURN && f->legBack == false )
			{
				f->legWaits = 1 - lead;
			}
			f->legBlocks[lead] = f->footSwings[lead] == false && holds;
		}
		f->legRoom = room;
	}
	if ( fabsf( turn ) <= room )
	{
		f->legHeld = 0.0f;
		return to;
	}
	// Free to turn as far as they do in this update, they do not wait
	if ( room >= b3MaxFloat( f->def.steering.turnRate, 0.0f ) * timeStep )
	{
		f->legHeld = 0.0f;
		return nbWrapAngle( from + ( turn > 0.0f ? room : -room ) );
	}
	f->legHeld += timeStep;
	if ( f->legHeld > NB_LEG_HOLD )
	{
		return to;
	}
	return nbWrapAngle( from + ( turn > 0.0f ? room : -room ) );
}

static void nbSteerFeet( const nbWorld* world, int index, nbFigure* f, const nbClipFoot* clips, b3Vec3 up, float timeStep )
{
	float scale = nbFigureScale( f );
	b3Vec3 velocity = b3MulSub( f->moveVelocity, b3Dot( f->moveVelocity, up ), up );
	float speed = b3Length( velocity );
	f->gaitSpeed += b3MinFloat( 1.0f, NB_GAIT_FOLLOW * timeStep ) * ( speed - f->gaitSpeed );
	f->gaitCrouch = 0.0f;
	f->gaitBob = 0.0f;

	// In the air the gait ends at once, before it crouches the root for this update, the clip takes the legs back, see
	// NB_FOOT_FADE, and the root goes on from where it was, see nbCarryRoot. Until round 104 the gait ended after the
	// crouch, and the root stood over the lower foot in the same update with the crouch still on it: the pelvis dropped up
	// to 27 cm for that one update.
	if ( f->onGround == false )
	{
		f->gaitOn = false;
	}

	// The feet aim with the velocity of the capsule as a critically damped spring follows it, see NB_GAIT_AIM_LAG
	if ( f->gaitOn )
	{
		b3Vec3 offset = b3Sub( f->gaitVelocity, velocity );
		nbStepSpring( &offset, &f->gaitAcceleration, 1.0f / NB_GAIT_AIM_LAG, 1.0f, timeStep );
		f->gaitVelocity = b3Add( velocity, offset );
	}
	else
	{
		f->gaitVelocity = velocity;
		f->gaitAcceleration = b3Vec3_zero;
	}
	bool wishes = b3LengthSquared( f->steer.velocity ) > NB_STEER_FACE_SPEED * NB_STEER_FACE_SPEED;
	b3Vec3 aimVelocity = wishes ? b3MulAdd( f->gaitVelocity, 2.0f * NB_GAIT_AIM_LAG, f->gaitAcceleration ) : f->gaitVelocity;
	b3Vec3 aimWish = b3MulSub( f->wishVelocity, b3Dot( f->wishVelocity, up ), up );
	b3Vec3 aimAcceleration = b3MulSub( f->gaitAcceleration, b3Dot( f->gaitAcceleration, up ), up );

	if ( f->gaitOn == false )
	{
		// Standing on the feet of the clip, any turn starts the gait, on feet that still stand where it stopped, a turn
		// farther than the feet may stand turned
		float limit = f->footWeight <= 0.0f ? 0.0f : NB_GAIT_TURN;
		bool turned = false;
		for ( int k = 0; k < 2; ++k )
		{
			turned = turned || fabsf( nbWrapAngle( f->footHeading[k] - f->heading ) ) > limit;
		}
		bool start = f->onGround && ( speed > NB_GAIT_START || turned || f->balancing );
		if ( start == false )
		{
			// Standing, the feet go where the clip puts them, and once they stand there, they go along with the frame, as the
			// legs do
			f->legHeading = nbWrapAngle( f->heading + f->legTurn );
			if ( f->footWeight <= 0.0f )
			{
				for ( int k = 0; k < 2; ++k )
				{
					f->feet[k].plant = nbClipAnkle( f, clips + k, f->feet + k );
					f->feet[k].tilt = nbPartOf( f->feet[k].lean, clips[k].weight );
					f->footHeading[k] = f->heading;
				}
			}
			return;
		}

		// The feet stand where they are, after a catch where it caught itself. The one toward where it goes lifts, into its
		// swing, as the other stands under the hip, as far into its stance as the hip passes over a foot.
		if ( f->balancing == false && f->footWeight <= 0.0f )
		{
			const int feet[2] = { nb_humanoidFootLeft, nb_humanoidFootRight };
			for ( int k = 0; k < 2; ++k )
			{
				f->feet[k].plant = f->targets[feet[k]].p;
				f->feet[k].tilt = nbPartOf( f->feet[k].lean, clips[k].weight );
			}
		}
		f->gaitOn = true;
		f->balancing = false;
		float run, stance, land;
		nbGaitSpeed( world, f, &run, &stance, &land );
		b3Vec3 local = b3InvRotateVector( f->frame.q, velocity );
		int first = local.x < 0.0f ? 1 : 0;
		f->gaitPhase = land * stance + ( first == 0 ? 0.0f : 0.5f );
		f->gaitPhase -= floorf( f->gaitPhase );
		f->gaitLag = 0.0f;
		f->gaitLagRate = 0.0f;
		f->gaitStarting = true;
		for ( int k = 0; k < 2; ++k )
		{
			f->footSwings[k] = false;
			f->canLift[k] = true;
			f->footRoll[k] = 0.0f;
			f->footRolled[k] = 0.0f;
			f->footRollGain[k] = 0.0f;
			f->heelPart[k] = 1.0f;
			f->landRoll[k] = 0.0f;
			f->footOut[k] = FLT_MAX;
			f->outBehind[k] = false;
			f->outToes[k] = false;
			f->swingHurry[k] = 1.0f;
			f->footholdChosen[k] = false;
			f->foothold[k] = b3OffsetPos( f->feet[k].plant, b3MulSV( clips[k].sole.y - clips[k].ankle.y, up ) );
			f->liftHold[k] = f->foothold[k];
		}
	}

	f->footWeight = 1.0f;
	f->swingFoot = -1;
	float gravity = b3MaxFloat( b3Length( b3World_GetGravity( world->physicsWorld ) ), 0.1f );
	float omega = sqrtf( gravity / b3MaxFloat( f->def.standHeight, 0.1f ) );
	float run, stance, land;
	float u = nbGaitSpeed( world, f, &run, &stance, &land );
	float springRun = f->runTurn;
	f->gaitRun = run;
	f->gaitUnit = sqrtf( gravity * b3MaxFloat( f->def.standHeight, 0.1f ) );

	// Starting to run, the feet plan with the acceleration of the capsule, see NB_GAIT_PLAN_FROM, as fast as the program
	// asks before the legs cap it
	b3Vec3 asked = b3MulSub( f->askedVelocity, b3Dot( f->askedVelocity, up ), up );
	f->askedSpeed = b3Length( asked );
	float uAim =
		b3MaxFloat( b3Length( aimVelocity ), b3Length( asked ) ) / sqrtf( gravity * b3MaxFloat( f->def.standHeight, 0.1f ) );
	float runs = b3ClampFloat( ( b3MaxFloat( run, springRun ) - NB_GAIT_PLAN_FROM ) / ( 1.0f - NB_GAIT_PLAN_FROM ), 0.0f, 1.0f );
	float fast = b3ClampFloat( ( uAim - NB_GAIT_RUN_TO ) / NB_GAIT_PLAN_SPAN, 0.0f, 1.0f );
	float plan =
		b3MaxFloat( runs * runs * ( 3.0f - 2.0f * runs ), fast * fast * ( 3.0f - 2.0f * fast ) ) * ( 1.0f - f->stairWeight );
	b3Vec3 startAcceleration = b3MulSV( b3MaxFloat( 1.0f - f->stairWeight, plan ), aimAcceleration );
	aimAcceleration = b3MulSV( plan, aimAcceleration );
	// Whether the figure wants to run, see NB_GAIT_RUN_FROM: walking, a foot left behind may then lift before the other
	// one lands
	bool wantsRun = b3Length( aimWish ) > NB_GAIT_RUN_FROM * sqrtf( gravity * b3MaxFloat( f->def.standHeight, 0.1f ) );

	// Not facing where it moves, its legs turn toward where its gait goes, see NB_LEG_TURN_WALK. Going, they keep where
	// they face in the world as the heading turns, and stopping, where they turned to; still, they turn with the heading
	// and back to it. The feet on the ground hold them back for a moment, see NB_LEG_KEEP.
	if ( f->steer.faceVelocity == false )
	{
		// Toward where the capsule goes NB_LEG_AIM_AHEAD later, and no farther than what it wants: past that, toward what
		// it wants
		b3Vec3 legVelocity = aimVelocity;
		if ( wishes )
		{
			b3Vec3 flat = b3MulSub( f->gaitVelocity, b3Dot( f->gaitVelocity, up ), up );
			b3Vec3 then =
				b3MulAdd( flat, NB_LEG_AIM_AHEAD, b3MulSub( f->gaitAcceleration, b3Dot( f->gaitAcceleration, up ), up ) );
			if ( b3Dot( b3Sub( then, aimWish ), b3Sub( flat, aimWish ) ) <= 0.0f )
			{
				then = aimWish;
			}
			legVelocity = then;
		}
		b3Vec3 local = b3InvRotateVector( f->frame.q, legVelocity );
		bool still = local.x * local.x + local.z * local.z <= NB_LEG_TURN_STILL * NB_LEG_TURN_STILL * scale * scale;
		if ( still )
		{
			f->legAim = 0.0f;
			f->legBack = false;
		}
		else if ( wishes )
		{
			float across;
			f->legAim = nbLegTurnFor( f, local, &f->legBack, &across );
		}
		float from = still ? nbWrapAngle( f->heading + f->legTurn ) : f->legHeading;
		float to = nbHoldLegs( f, from, nbWrapAngle( f->heading + f->legAim ), scale, timeStep );
		float legs = nbTurnToward( from, to, &f->legHeadingRate, &f->def.steering, timeStep );
		f->legTurn = b3ClampFloat( nbWrapAngle( legs - f->heading ), -NB_LEG_TURN_MOST, NB_LEG_TURN_MOST );
		f->legHeading = nbWrapAngle( f->heading + f->legTurn );
	}
	else
	{
		f->legTurn = 0.0f;
		f->legHeading = f->heading;
		f->legHeadingRate = 0.0f;
		f->legAim = 0.0f;
		f->legBack = false;
		f->legHeld = 0.0f;
		f->legRoom = FLT_MAX;
		f->legWaits = -1;
		f->legLead = 1;
		f->legBlocks[0] = false;
		f->legBlocks[1] = false;
	}
	float legHeading = f->legHeading;
	b3Quat legQ = f->legTurn != 0.0f ? b3NormalizeQuat( b3MulQuat( f->frame.q, nbHeadingQuat( f->legTurn ) ) ) : f->frame.q;
	// As they turn on, they let the capsule go faster, and the swings plan with it, see NB_PLAN_LOOK
	nbCapsulePlan capsulePlan;
	nbPlanCapsule( f, up, timeStep, &capsulePlan );
	// Starting backward, the first swing plans with the acceleration of the capsule until it lands, see NB_GAIT_PLAN_FROM
	bool startsBack = f->gaitStarting && f->legBack;
	b3Vec3 swingAcceleration = startsBack ? startAcceleration : aimAcceleration;
	float cadence = nbGaitCadence( f, omega, u, run );
	cadence *= 1.0f + NB_GAIT_STEEP * fabsf( f->slope );
	if ( f->legBack )
	{
		cadence *= 1.0f + ( 1.0f - run ) * NB_GAIT_BACK_CADENCE;
	}

	// On stairs a step goes a whole number of treads, see NB_STAIR_KEEP, and the cadence is as much faster or slower, and
	// the feet land flat
	float flatter = 1.0f;
	if ( f->stairWeight > 0.0f && f->stairDepth > 0.0f && speed > NB_GAIT_START )
	{
		// The step the gait would take on a ramp as steep as the stairs, in treads
		float steep = 1.0f + NB_GAIT_STEEP * fabsf( f->stairRise ) / f->stairDepth;
		float ratio = speed * ( 1.0f + NB_GAIT_STEEP * fabsf( f->slope ) ) / ( cadence * steep * f->stairDepth );
		int treads = f->stairStep;
		if ( treads < 1 || ratio < ( 1.0f - NB_STAIR_KEEP ) * sqrtf( (float)( ( treads - 1 ) * treads ) ) ||
			 ratio > ( 1.0f + NB_STAIR_KEEP ) * sqrtf( (float)( treads * ( treads + 1 ) ) ) )
		{
			treads = nbStairTreads( ratio, fabsf( f->stairRise ), scale );
		}
		f->stairStep = treads;
		cadence += f->stairWeight * ( speed / ( (float)treads * f->stairDepth ) - cadence );
		flatter = 1.0f - f->stairWeight;
	}
	float cycle = 2.0f / cadence;
	float ease = b3MinFloat( 1.0f, u / NB_GAIT_EASE );
	float heel = ( 1.0f - run ) * ease * NB_GAIT_HEEL;
	if ( flatter < 1.0f )
	{
		heel *= flatter;
	}
	// The cycle goes on at the cadence and catches up with the feet where they got ahead of it, see NB_GAIT_KEEP
	float keep = 1.0f / NB_GAIT_KEEP;
	float keepDecay = nbDecay( keep * timeStep );
	float keepSum = f->gaitLagRate + keep * f->gaitLag;
	float lag = keepDecay * ( f->gaitLag + timeStep * keepSum );
	f->gaitLagRate = keepDecay * ( f->gaitLagRate - keep * timeStep * keepSum );
	float catchUp = f->gaitLag - lag;
	f->gaitLag = lag;
	f->gaitPhase += timeStep * f->gaitRate / cycle + catchUp;
	f->gaitPhase -= floorf( f->gaitPhase );
	// How far ahead of the hip a foot lands, in seconds at the velocity it aims with, no farther than the gait would lead
	// at that speed, see NB_GAIT_PLAN_FROM
	float lead = land * stance * cycle;
	if ( f->stairWeight <= 0.0f )
	{
		lead = b3MinFloat( lead, nbGaitLead( world, f, b3Length( aimVelocity ) ) );
	}
	b3Vec3 ahead = b3MulSV( lead, aimVelocity );

	// On stairs the root goes up or down with the footholds under it, see nbStairGround, the ground under it no faster than
	// NB_STAIR_FOLLOW off the slope of the ground between the footholds around the hip, or of the stairs where it has
	// not both, so the body does not jump where the footholds change, and it does not go down or up the stairs before
	// the ground under it does, while the capsule still runs on level ground
	float lifted = 0.0f;
	if ( f->stairWeight > 0.0f )
	{
		b3Vec3 way = speed > 0.0f ? b3MulSV( 1.0f / speed, velocity ) : b3RotateVector( f->frame.q, b3Vec3_axisZ );
		b3Pos hip = b3TransformWorldPoint( f->frame, f->root.p );
		float steep = nbGaitSlope( f );
		float ground = nbStairGround( f, up, way, hip, &steep );
		float keep =
			b3Dot( b3SubPos( f->stairBase, f->frame.p ), up ) + steep * b3Dot( b3SubPos( f->frame.p, f->carryFrame ), way );
		float most = NB_STAIR_FOLLOW * scale * timeStep;
		lifted = b3ClampFloat( f->stairWeight * ground, keep - most, keep + most );
		f->root.p.y += lifted;
	}
	f->stairBase = b3OffsetPos( f->frame.p, b3MulSV( lifted, up ) );

	// Walking, the legs stretch farther and the root goes up as far, see NB_LEG_REACH_WALK, less as the capsule turns or slows
	// down, see NB_GAIT_BEND_SPEED
	const nbBone* thighs = f->bones + nb_humanoidThighLeft;
	float legLength = b3Length( thighs[1].rest.p ) + b3Length( thighs[2].rest.p );
	b3Vec3 wish = b3MulSub( f->wishVelocity, b3Dot( f->wishVelocity, up ), up );
	b3Vec3 change = b3Sub( wish, f->gaitVelocity );
	float moving = b3Length( f->gaitVelocity );
	float turnOrBrake = 0.0f;
	if ( moving > 0.0f )
	{
		b3Vec3 along = b3MulSV( 1.0f / moving, f->gaitVelocity );
		float onward = b3Dot( change, along );
		turnOrBrake = b3Length( b3MulSub( change, onward, along ) ) + b3MaxFloat( -onward, 0.0f );
	}
	float bend = b3MinFloat( turnOrBrake / ( NB_GAIT_BEND_SPEED * scale ), 1.0f );
	float bendOmega = bend > f->gaitBend ? 1.0f / NB_GAIT_BEND_IN : 1.0f / NB_GAIT_BEND_OUT;
	float bendDecay = nbDecay( bendOmega * timeStep );
	float bendOff = f->gaitBend - bend;
	float bendSum = f->gaitBendRate + bendOmega * bendOff;
	f->gaitBend = b3ClampFloat( bend + bendDecay * ( bendOff + timeStep * bendSum ), 0.0f, 1.0f );
	f->gaitBendRate = bendDecay * ( f->gaitBendRate - bendOmega * timeStep * bendSum );
	float stretch = NB_LEG_REACH_WALK - NB_LEG_REACH - f->gaitBend * ( NB_LEG_REACH_WALK - NB_LEG_REACH_BENT );
	f->legReach = NB_LEG_REACH + ( 1.0f - run ) * ease * stretch;
	bool clipRoot = nbGaitClipRooted( f );
	if ( clipRoot )
	{
		// With clips of its gait that move the root, the root goes up and down as they have it instead, see nbGaitClipPose,
		// on stairs at the height they have it on average, as the ground under it goes up and down, and it is as much lower
		// than the gait of the library would have it as they crouch, see nbLowerSteered
		float clipHigh = f->gaitRoot.p.y + f->stairWeight * ( f->gaitClipLevel - f->gaitRoot.p.y );
		f->root.p.y += clipHigh;
		f->gaitClipCrouch = b3MaxFloat( ( f->legReach - NB_LEG_REACH ) * legLength - clipHigh, 0.0f );
	}
	else
	{
		float lift = ( f->legReach - NB_LEG_REACH ) * legLength;
		f->root.p.y += f->gaitBlend * lift;
	}

	// How low the root goes for the leg of each foot to reach where it lands, from the hip as the clip has it, the ground
	// there as much higher as the slope rises that far ahead
	b3Vec3 aheadInFrame = b3InvRotateVector( f->frame.q, ahead );
	float slope = nbGaitSlope( f );
	float rise = slope * b3Length( ahead );
	float needs[2];
	float levels[2];
	b3Vec3 hips[2];
	float reaches[2];
	b3Vec3 onHeel[2];
	float landSide = f->legBack ? -1.0f : 1.0f;
	for ( int k = 0; k < 2; ++k )
	{
		const nbBone* thigh = f->bones + ( k == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight );
		float length = b3Length( thigh[1].rest.p ) + b3Length( thigh[2].rest.p );
		float reach = f->legReach * length;
		b3Vec3 hip = b3TransformPoint( f->root, thigh->rest.p );

		// Walking, a foot lands on its heel, its ankle back and up from where it stands flat, and going backward on its toes,
		// its ankle up, see nbRollFoot and NB_GAIT_HEEL
		onHeel[k] = b3Vec3_zero;
		b3Quat heelRotation = clips[k].rotation;
		nbRollFoot( thigh + 2, -landSide * heel, 0.0f, onHeel + k, &heelRotation );

		float narrow = 1.0f - NB_GAIT_NARROW * run - NB_GAIT_NARROW_WALK * ( 1.0f - run ) * ease;
		float x = clips[k].ankle.x * narrow + aheadInFrame.x + onHeel[k].x - hip.x;
		float z = clips[k].ankle.z + aheadInFrame.z + onHeel[k].z - hip.z;
		float across = b3MinFloat( x * x + z * z, reach * reach );
		levels[k] = f->root.p.y - hip.y + clips[k].ankle.y + onHeel[k].y + sqrtf( reach * reach - across );
		needs[k] = levels[k] + rise;
		hips[k] = hip;
		reaches[k] = reach;
	}
	// Running, the root goes as low as the legs need to reach both feet where they land and bobs with the steps, see
	// NB_RUN_TURN_LAG. With clips of its gait that move the root, it runs as low and bobs as they have it.
	float crouch =
		clipRoot ? 0.0f : b3MinFloat( run, springRun ) * b3MaxFloat( f->root.p.y - b3MinFloat( needs[0], needs[1] ), 0.0f );
	float steady = crouch;
	float bob = clipRoot ? 0.0f : ease * springRun * NB_GAIT_BOB_RUN * scale;
	float bobStance = nbGaitStance( f, springRun );

	float swingTime = ( 1.0f - stance ) * cycle;

	// A foot on the ground gets lost soon where its leg would need the root lower than NB_GAIT_LOSE below where the gait
	// has it before the other foot could swing to the ground, beside or ahead of the hip, and where it holds back the legs
	// as they turn, see NB_LEG_BLOCK. A swinging foot hurries to land before the foot on the ground gets lost, and
	// walking, before the leg of a foot behind the hip would need the root lower than it went, rolled over its toes as far
	// as it can.
	float quickest = swingTime / ( NB_GAIT_HURRY * f->gaitRate );
	bool runsBack = f->legBack && stance < 0.5f;
	bool trails[2];
	bool lostSoon[2];
	float out[2];
	for ( int k = 0; k < 2; ++k )
	{
		// A foot trails where the hip goes away from it: one behind the hip, and going backward one ahead of it, see
		// nbLowerSteered
		trails[k] = f->outBehind[k];
		out[k] = f->footSwings[k] ? FLT_MAX : b3MaxFloat( f->footOut[k] - timeStep, 0.0f );
		// One that only just landed does not lift again for the legs, see NB_LEG_BLOCK
		float phase = f->gaitPhase + ( k == 1 ? 0.5f : 0.0f );
		phase -= floorf( phase );
		bool blocks = f->legBlocks[k] && phase >= NB_GAIT_FRESH;
		lostSoon[k] = ( trails[k] == false && out[k] < quickest ) || blocks;
	}
	for ( int k = 0; k < 2; ++k )
	{
		int j = 1 - k;
		float footTime = nbSwingTime( f, k, swingTime, cycle, up );
		if ( f->footSwings[k] && out[j] < FLT_MAX && ( trails[j] == false || stance >= 0.5f ) )
		{
			float rest = ( 1.0f - f->swingAlong[k] ) * footTime / f->swingRate[k];
			if ( rest > f->swingHurry[k] * out[j] )
			{
				float want = out[j] * NB_GAIT_HURRY > rest ? rest / out[j] : NB_GAIT_HURRY;
				want = b3MinFloat( want, nbMostHurry( f->feet + k, up, f->swingStart[k], footTime, f->swingRate[k] ) );
				want = b3MinFloat( want, f->swingHurry[k] + NB_GAIT_HURRY_RATE * timeStep );
				f->swingHurry[k] = b3MaxFloat( f->swingHurry[k], want );
			}
		}
		// The foot the legs wait for lands sooner, see NB_LEG_WAIT_HURRY
		if ( f->footSwings[k] && f->legWaits == k )
		{
			float want =
				b3MinFloat( NB_LEG_WAIT_HURRY, nbMostHurry( f->feet + k, up, f->swingStart[k], footTime, f->swingRate[k] ) );
			want = b3MinFloat( want, f->swingHurry[k] + NB_GAIT_HURRY_RATE * timeStep );
			f->swingHurry[k] = b3MaxFloat( f->swingHurry[k], want );
		}
	}
	// Which foot wants to lift now: one that gets lost soon or whose phase came round past its stance
	bool wants[2];
	for ( int k = 0; k < 2; ++k )
	{
		float phase = f->gaitPhase + ( k == 1 ? 0.5f : 0.0f );
		phase -= floorf( phase );
		wants[k] = f->footSwings[k] == false && ( lostSoon[k] || ( f->canLift[k] && phase >= stance ) );
	}
	bool settled = f->legTurn == 0.0f;
	for ( int k = 0; k < 2; ++k )
	{
		nbFoot* foot = f->feet + k;
		float footTime = nbSwingTime( f, k, swingTime, cycle, up );
		float phase = f->gaitPhase + ( k == 1 ? 0.5f : 0.0f );
		phase -= floorf( phase );
		if ( f->footSwings[k] )
		{
			// A swing takes its time, whatever the cycle does meanwhile, and the foot lands where it aimed. It lifts again once
			// its phase came round past its stance.
			f->swingAlong[k] += timeStep * f->swingRate[k] * f->swingHurry[k] / footTime;
			if ( f->swingAlong[k] >= 1.0f )
			{
				// It lands, walking on its heel, and stepping backward on its toes, see NB_GAIT_HEEL. A foot that found no edge
				// to land beyond makes the stairs less sure.
				if ( f->footholdChosen[k] == false && f->stairWeight > 0.0f )
				{
					f->stairWeight = f->stairWeight > 0.2f ? 0.5f * f->stairWeight : 0.0f;
					if ( f->stairWeight == 0.0f )
					{
						f->stairDepth = 0.0f;
						f->stairStep = 0;
					}
				}
				f->footholdChosen[k] = false;
				f->footSwings[k] = false;
				f->gaitStarting = false;
				f->canLift[k] = phase < stance;
				float stepSide = f->stepBack[k] ? -1.0f : 1.0f;
				float landed = -stepSide * f->heelPart[k] * heel;
				f->footRoll[k] = landed;
				f->footRolled[k] = landed;
				f->footRollGain[k] = 0.0f;
				f->landRoll[k] = landed;
				f->stanceTime[k] = 0.0f;
				foot->plant = foot->target;
				foot->tilt = foot->targetTilt;
				f->footHeading[k] = f->landHeading[k];
			}
		}
		else
		{
			// A planted foot turns on the middle of its sole with the body beyond NB_FOOT_TWIST from where the legs face
			float twist = nbWrapAngle( f->footHeading[k] - legHeading );
			if ( fabsf( twist ) > NB_FOOT_TWIST )
			{
				float turn = ( twist > 0.0f ? NB_FOOT_TWIST : -NB_FOOT_TWIST ) - twist;
				b3Vec3 sole = b3Sub( clips[k].sole, clips[k].ankle );
				sole.y = 0.0f;
				b3Vec3 toSole = b3RotateVector( legQ, b3RotateVector( nbHeadingQuat( twist ), sole ) );
				b3Pos middle = b3OffsetPos( foot->plant, toSole );
				b3Quat spin = b3MakeQuatFromAxisAngle( up, turn );
				foot->plant = b3OffsetPos( middle, b3RotateVector( spin, b3Neg( toSole ) ) );
				f->footHeading[k] = nbWrapAngle( f->footHeading[k] + turn );
			}

			// A foot that landed on its heel, or on its toes, comes down from it, see NB_GAIT_HEEL_LOAD
			f->stanceTime[k] += timeStep;
			if ( f->landRoll[k] != 0.0f )
			{
				float down = f->stanceTime[k] / ( NB_GAIT_HEEL_LOAD * cycle );
				if ( down < 1.0f )
				{
					f->footRoll[k] = f->landRoll[k] * ( 1.0f - down * down * ( 3.0f - 2.0f * down ) );
				}
				else
				{
					f->footRoll[k] = 0.0f;
					f->landRoll[k] = 0.0f;
				}
			}

			f->canLift[k] = f->canLift[k] || phase < stance;
			float before = ( stance - phase ) / NB_GAIT_PRESWING;
			float push = b3ClampFloat( 1.0f - before, 0.0f, 1.0f );
			push = push * push * push * ( 10.0f + push * ( 6.0f * push - 15.0f ) );
			// It pushes no faster than over the preswing at the cadence, also where the cycle gets shorter as the
			// figure starts to run and its phase comes into the preswing at once
			float pushMost = f->footPush[k] + NB_GAIT_PUSH * timeStep / ( NB_GAIT_PRESWING * cycle );
			float pushWant = ( 1.0f - run ) * ease * NB_GAIT_PUSH * push;
			f->footPush[k] = f->canLift[k] ? b3MaxFloat( f->footPush[k], b3MinFloat( pushWant, pushMost ) ) : 0.0f;

			// A foot behind the hip that its leg would not reach in the next update even rolled over its toes as far as it can
			// lifts early, see nbLowerSteered, and one its leg no longer reaches at all lifts wherever its phase is. A foot
			// that gets lost soon lifts while the other foot stands, before that one. Of two feet on the ground the one its
			// leg loses first lifts first once one of them has to: the other waits, even where its phase says it is due,
			// but not a foot that only just landed, see NB_GAIT_FRESH. Walking, a foot stands while the other swings, but where
			// the figure wants to run and the leg no longer reaches it behind the hip even rolled over its toes, it lifts, and
			// for a moment both feet are off the ground, as where a walker breaks into a run: it pulled the root down after it,
			// without the root coming down more softly, see nbLowerSteered, 12 cm in two updates running round about.
			bool hold =
				f->footSwings[1 - k] && stance >= 0.5f && ( wantsRun == false || trails[k] == false || f->footOut[k] > 0.0f );
			bool early = ( f->canLift[k] || f->footOut[k] <= 0.0f ) && trails[k] && f->footOut[k] <= timeStep;
			// Walking too, a trailing foot or one that stays on its toes that its leg loses in the next update lifts where
			// the other foot lands within NB_GAIT_FLOAT
			bool floats = false;
			if ( f->footSwings[1 - k] && f->footOut[k] <= timeStep )
			{
				float otherTime = nbSwingTime( f, 1 - k, swingTime, cycle, up );
				float rest = ( 1.0f - f->swingAlong[1 - k] ) * otherTime / ( f->swingRate[1 - k] * f->swingHurry[1 - k] );
				if ( rest <= NB_GAIT_FLOAT )
				{
					hold = hold && early == false;
					floats = f->outToes[k];
				}
			}
			bool blocked = f->footSwings[1 - k] == false && out[1 - k] < out[k] && out[1 - k] < quickest;
			bool lost = lostSoon[k] && f->footSwings[1 - k] == false && blocked == false;
			bool due = f->canLift[k] && phase >= stance && lostSoon[1 - k] == false && blocked == false;
			bool first =
				f->canLift[k] && f->footSwings[1 - k] == false && out[k] < quickest && out[k] < out[1 - k] && wants[1 - k];
			float otherPhase = f->gaitPhase + ( k == 1 ? 0.0f : 0.5f );
			otherPhase -= floorf( otherPhase );
			bool otherFresh = f->footSwings[1 - k] == false && otherPhase < NB_GAIT_FRESH;
			bool otherGoing = f->footSwings[1 - k] == false && trails[1 - k] && out[1 - k] < quickest;
			if ( phase < NB_GAIT_FRESH && lostSoon[k] && otherGoing )
			{
				lost = false;
				first = false;
			}
			if ( otherFresh && lostSoon[1 - k] && trails[k] && out[k] < quickest && f->canLift[k] )
			{
				first = true;
			}
			if ( ( ( due || early ) && hold == false ) || lost || first || floats )
			{
				// It lifts from where it stands, as far into its swing as its phase, and swings in the rest of it, from where it
				// was an update ago, so it moves in this update already
				f->footSwings[k] = true;
				f->canLift[k] = false;
				// Lifting more than an update before its phase says, it shows how far the cycle is behind, see NB_GAIT_KEEP
				float off = phase - stance;
				off -= floorf( off + 0.5f );
				if ( off < -timeStep * f->gaitRate / cycle )
				{
					f->gaitLag = ( 1.0f - run ) * b3MinFloat( -off, NB_GAIT_KEEP_MOST );
				}
				// A late swing hurries to land when its phase comes round. Running, an early one swings as much slower,
				// up to NB_GAIT_KEEP_MOST of a cycle later, see NB_GAIT_KEEP. A foot that lifts more than half a cycle
				// after its phase says is nearer to its next swing than to this one: it is early for that one, not late.
				// Taken as late, as until round 122, it hurried as much as it could and still landed half a cycle off its
				// phase, beside the other foot, whose swing had waited for the cycle: from then on both feet lifted
				// almost together and one of them hurried at every step, for seconds. Turning about at a run, where both
				// feet lift up to 0.4 of a cycle before their phase as the figure runs off again, that happened after 13
				// of 336 turns, and a shin jumped up to 60 mm in the runs that ended so, see SteerTurnRhythmTest.
				float ahead = phase - stance;
				if ( ahead > 0.5f )
				{
					ahead -= 1.0f;
				}
				float slowest = -run * NB_GAIT_KEEP_MOST / ( 1.0f - stance );
				float late = b3ClampFloat( ahead / ( 1.0f - stance ), slowest, NB_GAIT_AIM );
				f->swingAlong[k] = 0.0f;
				f->swingRate[k] = 1.0f;
				f->swingHurry[k] = late * NB_GAIT_HURRY < NB_GAIT_HURRY - 1.0f ? 1.0f / ( 1.0f - late ) : NB_GAIT_HURRY;
				f->swingStart[k] = -timeStep * f->swingRate[k] * f->swingHurry[k] / footTime;
				f->swingHeight[k] = ( NB_GAIT_LIFT_WALK + ( NB_GAIT_LIFT_RUN - NB_GAIT_LIFT_WALK ) * springRun ) * scale;
				f->swingRun[k] = run;
				f->swingBack[k] = runsBack;
				f->stepBack[k] = f->legBack;
				f->swingShare[k] = 1.0f - stance;
				f->swingLift[k] = swingTime;
				f->swingNarrow[k] = 1.0f - NB_GAIT_NARROW * run - NB_GAIT_NARROW_WALK * ( 1.0f - run ) * ease;
				foot->from = foot->plant;
				foot->fromTilt = foot->tilt;
				f->swingBase[k] = foot->plant;
				f->swingVelocity[k] = b3Vec3_zero;
				int thigh = k == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight;
				float stretch = b3Length( b3SubPos( f->targets[thigh].p, f->targets[thigh + 2].p ) ) / legLength;
				float soft =
					b3ClampFloat( ( stretch - NB_SWING_SOFT_FROM ) / ( NB_SWING_SOFT_TO - NB_SWING_SOFT_FROM ), 0.0f, 1.0f );
				f->swingSoft[k] = f->footRoll[k] > 0.0f ? soft * soft * ( 3.0f - 2.0f * soft ) : 0.0f;
				f->swingTurnRate[k] = 0.0f;
				f->liftHold[k] = f->foothold[k];
				f->liftHeading[k] = f->footHeading[k];
				f->swingRoll[k] = f->footRoll[k];
				f->footPush[k] = 0.0f;
				f->pushPast[k] = 0.0f;
				foot->target = foot->plant;
				foot->targetTilt = foot->tilt;
				f->targetGround[k] = true;
			}
		}

		if ( f->footSwings[k] )
		{
			float swing = f->swingAlong[k];
			float natural = ( 1.0f - swing ) * footTime;
			float left = natural / ( f->swingRate[k] * f->swingHurry[k] );
			f->swingRest[k] = left;
			if ( swing < NB_GAIT_AIM + NB_GAIT_AIM_FADE && left > NB_GAIT_AIM_SETTLED )
			{
				// Where the hip will pass over it as far into its stance as land, beside the body, on the ground there, no
				// farther ahead than the gait leads at the speed the frame has as it lands, see NB_GAIT_PLAN_FROM: where the
				// figure turned from running backward to running forward, the gait walked for a moment as its speed went
				// through zero, a foot aimed with the lead of walking at 3 m/s, 0.7 instead of 0.26 m ahead of the hip, and
				// the root went down 31 cm for the leg to reach it
				b3Vec3 landVelocity;
				b3Vec3 way = nbFrameAhead( aimVelocity, swingAcceleration, aimWish, left, &landVelocity );
				b3Vec3 ownWay = way;
				b3Vec3 ownVelocity = landVelocity;
				way = nbPlanAhead( f, &capsulePlan, left, plan, way, &landVelocity );
				float leadThen = lead;
				if ( f->stairWeight <= 0.0f )
				{
					leadThen = b3MinFloat( leadThen, nbGaitLead( world, f, b3Length( landVelocity ) ) );
				}
				way = b3MulAdd( way, leadThen, landVelocity );
				// How much farther it lands as the legs turn on and let the capsule go faster, see NB_PLAN_LOOK
				b3Vec3 farther = b3Vec3_zero;
				if ( capsulePlan.count > 0 )
				{
					float ownLead = b3MinFloat( lead, nbGaitLead( world, f, b3Length( ownVelocity ) ) );
					farther = b3Sub( way, b3MulAdd( ownWay, ownLead, ownVelocity ) );
				}
				float guess = slope * b3Length( way );
				float deeper = f->moverLow - f->moverRadius;
				b3WorldTransform end = { b3OffsetPos( f->frame.p, b3MulAdd( way, guess, up ) ), legQ };
				// Turned as far as the legs turn toward where they turn to until it lands, at the turn rate of the
				// steering, and the foot that does not step out for the legs, see NB_LEG_LEAD_SPEED, no farther than the
				// feet let the legs turn now, see NB_LEG_KEEP: planned to land turned about, it stepped round the foot on
				// the ground against the turn
				float landLegs = legHeading;
				float wants = nbWrapAngle( f->legAim - f->legTurn );
				float most = b3MaxFloat( f->def.steering.turnRate, 0.0f ) * left;
				if ( k != f->legLead )
				{
					most = b3MinFloat( most, f->legRoom );
				}
				float ahead = b3ClampFloat( wants, -most, most );
				if ( ahead != 0.0f )
				{
					end.q = b3NormalizeQuat( b3MulQuat( f->frame.q, nbHeadingQuat( f->legTurn + ahead ) ) );
					landLegs = nbWrapAngle( legHeading + ahead );
				}
				nbClipFoot aim = clips[k];
				aim.ankle.x *= f->swingNarrow[k];
				aim.sole.x *= f->swingNarrow[k];

				// With its whole sole on one tread, see nbFindFoothold, the one it chose for the swing while it stays near, and
				// the gait goes faster or slower so that the hip passes over it as it would where the gait puts it
				b3Vec3 forward = b3RotateVector( end.q, b3RotateVector( aim.rotation, b3Vec3_axisY ) );
				forward = b3Normalize( b3MulSub( forward, b3Dot( forward, up ), up ) );
				b3Pos middle = b3TransformWorldPoint( end, aim.sole );
				{
					// It lands on its own side of the other foot, see NB_GAIT_SIDE_GAP
					int j = 1 - k;
					b3Pos otherAnkle = f->footSwings[j] ? f->feet[j].target : f->feet[j].plant;
					float otherHeading = f->footSwings[j] ? f->landHeading[j] : f->footHeading[j];
					b3Vec3 toSole = b3Sub( clips[j].sole, clips[j].ankle );
					toSole.y = 0.0f;
					toSole = b3RotateVector(
						f->frame.q, b3RotateVector( nbHeadingQuat( nbWrapAngle( otherHeading - f->heading ) ), toSole ) );
					b3Pos other = b3OffsetPos( otherAnkle, toSole );
					b3Vec3 side = b3RotateVector( end.q, b3Vec3_axisX );
					side = b3Normalize( b3MulSub( side, b3Dot( side, up ), up ) );
					float sign = k == 0 ? 1.0f : -1.0f;
					float lack = NB_GAIT_SIDE_GAP * scale - sign * b3Dot( b3SubPos( middle, other ), side );
					if ( lack > 0.0f )
					{
						b3Vec3 move = b3MulSV( sign * lack, side );
						end.p = b3OffsetPos( end.p, move );
						middle = b3OffsetPos( middle, move );
					}
					if ( ahead != 0.0f )
					{
						// Landing turned ahead, its ankle keeps its side also across the legs less turned, as the pelvis follows
						// them, see NB_LAND_LAG
						float less = ahead - b3ClampFloat( ahead, -NB_LAND_LAG, NB_LAND_LAG );
						b3Quat lagQ = b3NormalizeQuat( b3MulQuat( f->frame.q, nbHeadingQuat( f->legTurn + less ) ) );
						b3Vec3 sideLag = b3RotateVector( lagQ, b3Vec3_axisX );
						sideLag = b3Normalize( b3MulSub( sideLag, b3Dot( sideLag, up ), up ) );
						b3Pos ankleThen = b3TransformWorldPoint( end, aim.ankle );
						float lackLag = NB_LAND_LAG_KEEP * scale - sign * b3Dot( b3SubPos( ankleThen, otherAnkle ), sideLag );
						if ( lackLag > 0.0f )
						{
							b3Vec3 move = b3MulSV( sign * lackLag, sideLag );
							end.p = b3OffsetPos( end.p, move );
							middle = b3OffsetPos( middle, move );
						}
					}
					if ( capsulePlan.count > 0 )
					{
						// What the legs letting the capsule go adds, see NB_PLAN_LOOK, does not take it across the other foot,
						// across the legs as they are now, or no farther across than it lands without it
						b3Vec3 sideNow = b3RotateVector( legQ, b3Vec3_axisX );
						sideNow = b3Normalize( b3MulSub( sideNow, b3Dot( sideNow, up ), up ) );
						float across = sign * b3Dot( b3SubPos( middle, other ), sideNow );
						float least = b3MinFloat( across - sign * b3Dot( farther, sideNow ), 0.0f );
						if ( across < least )
						{
							b3Vec3 move = b3MulSV( sign * ( least - across ), sideNow );
							end.p = b3OffsetPos( end.p, move );
							middle = b3OffsetPos( middle, move );
						}
					}
				}
				float shift = 0.0f;
				if ( f->footholdChosen[k] )
				{
					// It keeps the tread while the gait puts the foot near it or it can still land there in time, see
					// NB_STAIR_RATE, but not once the figure slows down short of it
					shift = b3Dot( b3SubPos( f->foothold[k], middle ), forward );
					float keep = f->stairDepth > 0.0f ? 0.5f * f->stairDepth : NB_FOOTHOLD_REACH * scale;
					float along = b3Dot( velocity, forward );
					float pace = along > NB_GAIT_START ? nbStairPace( natural, left, shift, along ) : 0.0f;
					bool inTime = pace >= 1.0f - NB_STAIR_RATE && pace <= 1.0f + NB_STAIR_RATE;
					// and late in the swing, as it keeps more and more of where it aimed, it keeps the tread, so it
					// does not land between two treads as the step the gait wants changes, see NB_GAIT_AIM_FADE
					f->footholdChosen[k] = fabsf( shift ) <= keep || inTime || swing > NB_GAIT_AIM - NB_GAIT_AIM_FADE;
				}
				if ( f->footholdChosen[k] == false )
				{
					shift = 0.0f;
					if ( nbFindFoothold( world, index, f, k, middle, forward, up, guess, deeper, scale, &shift ) )
					{
						f->footholdChosen[k] = true;
						f->foothold[k] = b3OffsetPos( middle, b3MulSV( shift, forward ) );
					}
				}
				if ( f->footholdChosen[k] )
				{
					end.p = b3OffsetPos( end.p, b3MulSV( shift, forward ) );
					float along = b3Dot( velocity, forward );
					if ( along > NB_GAIT_START )
					{
						float pace = nbStairPace( natural, left, shift, along );
						f->swingRate[k] = b3ClampFloat( pace, 1.0f - NB_STAIR_RATE, 1.0f + NB_STAIR_RATE );
					}
				}
				float lift = 0.0f;
				b3Quat tilt = b3Quat_identity;
				bool ground =
					nbFindFootGround( world, index, end, &aim, scale, guess, f->footholdChosen[k] ? deeper : 0.0f, &lift, &tilt );
				if ( ground && f->stairWeight == 0.0f )
				{
					ground = nbChooseFoothold( world, index, f, k, &aim, forward, up, guess, f->footholdChosen[k] ? deeper : 0.0f,
											   deeper, scale, &end, &lift, &tilt );
				}
				if ( ground == false )
				{
					// Without ground to stand on it aims down from the way of the frame, without the slope it guessed, as
					// deep again as the figure steps, or else at that way, never where the slope put it up in the air
					b3WorldTransform low = end;
					low.p = b3OffsetPos( end.p, b3MulSV( -guess, up ) );
					ground = nbFindFootGround( world, index, low, &aim, scale, 0.0f, deeper, &lift, &tilt ) &&
							 ( f->stairWeight > 0.0f || lift <= deeper );
					lift = ground ? lift - guess : -guess;
					tilt = ground ? tilt : b3Quat_identity;
				}
				b3Pos target = b3TransformWorldPoint( end, nbTiltedAnkle( &aim, lift, tilt ) );
				if ( swing <= f->swingStart[k] || swing < NB_GAIT_DESCEND || f->targetGround[k] == false ||
					 b3Dot( b3SubPos( target, foot->target ), up ) > -NB_GAIT_LEVEL * scale )
				{
					f->targetGround[k] = ground;
					// Late in the swing and before it lands it keeps more and more of where it aimed, see NB_GAIT_AIM_FADE and
					// NB_GAIT_AIM_SETTLE, the first swing starting backward from earlier on as far as it walks, see
					// NB_GAIT_AIM_SETTLE_START
					float kept = 0.5f * ( swing - NB_GAIT_AIM + NB_GAIT_AIM_FADE ) / NB_GAIT_AIM_FADE;
					kept = b3ClampFloat( kept, 0.0f, 1.0f );
					kept = kept * kept * ( 3.0f - 2.0f * kept );
					float settleTime =
						NB_GAIT_AIM_SETTLE +
						( startsBack ? ( 1.0f - plan ) * ( NB_GAIT_AIM_SETTLE_START - NB_GAIT_AIM_SETTLE ) : 0.0f );
					float settle = b3ClampFloat( ( settleTime - left ) / ( settleTime - NB_GAIT_AIM_SETTLED ), 0.0f, 1.0f );
					kept = b3MaxFloat( kept, settle * settle * ( 3.0f - 2.0f * settle ) );
					foot->target = b3OffsetPos( target, b3MulSV( kept, b3SubPos( foot->target, target ) ) );
					foot->targetTilt = b3NLerp( tilt, foot->targetTilt, kept );
					f->landHeading[k] = nbWrapAngle( landLegs + kept * nbWrapAngle( f->landHeading[k] - landLegs ) );
					f->foothold[k] = b3TransformWorldPoint( end, (b3Vec3){ aim.sole.x, aim.sole.y + lift, aim.sole.z } );
				}
			}
			if ( swing == 0.0f )
			{
				// As it lifts, a late swing hurries no more than its foot may
				f->swingHurry[k] = b3MinFloat( f->swingHurry[k], nbMostHurry( foot, up, 0.0f, footTime, f->swingRate[k] ) );
				f->swingStart[k] = -timeStep * f->swingRate[k] * f->swingHurry[k] / footTime;
				f->swingCalm[k] = b3MinFloat( 1.0f / ( f->swingHurry[k] * f->swingHurry[k] ), 1.0f );
			}
			float along = b3ClampFloat( ( swing - f->swingStart[k] ) / ( 1.0f - f->swingStart[k] ), 0.0f, 1.0f );
			// A swing that comes to hurry more than as it lifted lifts and turns its foot less far for that too while its
			// early bump still rises, the share of the bump it has going on as fast as in the last update at most, see
			// NB_SWING_EARLY_TOP
			if ( swing > 0.0f && along < NB_SWING_EARLY_TOP )
			{
				float want = b3MinFloat( 1.0f / ( f->swingHurry[k] * f->swingHurry[k] ), 1.0f );
				if ( want < f->swingCalm[k] )
				{
					float now = nbSwingEarly( along );
					float keep = now > 0.0f ? ( 2.0f * f->swingBump[k] - f->swingBumpWas[k] ) / now : want;
					f->swingCalm[k] = b3MaxFloat( want, b3MinFloat( f->swingCalm[k], keep ) );
				}
			}
			f->swingBumpWas[k] = swing > 0.0f ? f->swingBump[k] : 0.0f;
			f->swingBump[k] = f->swingCalm[k] * nbSwingEarly( along );
			float smooth = along * along * ( 3.0f - 2.0f * along );
			b3Vec3 step = b3SubPos( foot->target, foot->from );
			float higher = b3MaxFloat( b3Dot( step, up ), 0.0f );
			float across = b3Length( b3MulSub( step, b3Dot( step, up ), up ) );
			float height = b3MinFloat( f->swingHeight[k], NB_GAIT_LIFT_LOW * scale + 0.5f * across );
			// It does not lift lower as where it lands comes nearer where it lifted, as where the figure turns round and
			// where it lands passes back by there
			if ( swing > 0.0f )
			{
				height = b3MaxFloat( height, f->swingTop[k] );
				height = b3MinFloat( height, f->swingTop[k] + NB_GAIT_LIFT_RISE * scale * timeStep );
			}
			f->swingTop[k] = height;
			// A swing that hurries lifts and turns its foot early as fast as the gait, less far, see NB_GAIT_HURRY, as far as it
			// hurried as it lifted and came to hurry while its bump rose, see NB_SWING_EARLY_TOP: hurrying more later, it does
			// not lift the foot less at once
			float calm = f->swingCalm[k];
			float bump = nbSwingEarly( along );
			// A leg that lifted stretched over its toes lifts its foot gently at first, see NB_SWING_SOFT_FROM
			float lift =
				nbSwingLift( along, nbLiftShare( f->swingRun[k], springRun, along ), height, higher, calm, f->swingBack[k] ) *
				( 1.0f + f->swingSoft[k] * ( nbSmoothRamp( b3MinFloat( along / NB_SWING_SOFT_RAMP, 1.0f ) ) - 1.0f ) );
			// It goes to where it aims and turns to the heading it lands with on paths planned anew in every update, see
			// nbSwingPath, over the time its swing has left
			// A swing hurries no more than its foot may for the way it has left: where the cubic to where it aims, see
			// nbSwingPath, would brake harder than NB_GAIT_SWING_ACCEL as the foot lands over the time the swing has left, it
			// hurries less, no slower than its gait. Until round 118 that bound held as a swing lifted and where its hurry rose,
			// for its whole step: turning about at a run, where a foot lands moves far while it swings, a hurrying foot caught
			// up with it over its last updates and braked up to 96 mm in an update as it landed. At speeds that had not run
			// before, feet jump more than 37 mm in an update at 16 instead of 40 of 312 points facing where it moves and at 14
			// instead of 24 of 1 008 with a fixed heading, the hardest 53.8 instead of 77.5 mm. Let go slower than its gait as
			// well, the pelvis jumped 33.7 instead of 16.1 mm in the hardest turn.
			if ( swing > 0.0f && f->swingHurry[k] > 1.0f )
			{
				b3Vec3 toGo = b3SubPos( foot->target, f->swingBase[k] );
				toGo = b3MulSub( toGo, b3Dot( toGo, up ), up );
				float way = b3Length( toGo );
				if ( way > 0.0f )
				{
					float speed = b3MaxFloat( b3Dot( f->swingVelocity[k], toGo ) / way, 0.0f );
					float needs = ( sqrtf( speed * speed + 6.0f * NB_GAIT_SWING_ACCEL * way ) - speed ) / NB_GAIT_SWING_ACCEL;
					float has = ( 1.0f - swing ) * footTime / ( f->swingRate[k] * f->swingHurry[k] ) + timeStep;
					if ( has < needs )
					{
						float hurry =
							( 1.0f - swing ) * footTime / ( f->swingRate[k] * b3MaxFloat( needs - timeStep, timeStep ) );
						f->swingHurry[k] = b3MinFloat( f->swingHurry[k], b3MaxFloat( hurry, 1.0f ) );
					}
				}
			}
			float until = ( 1.0f - swing ) * footTime / ( f->swingRate[k] * f->swingHurry[k] );
			b3Vec3 toward = b3SubPos( foot->target, f->swingBase[k] );
			b3Vec3 move = nbSwingPath( toward, f->swingVelocity + k, until + timeStep, timeStep );
			f->swingBase[k] = b3OffsetPos( f->swingBase[k], move );
			foot->plant = b3OffsetPos( f->swingBase[k], b3MulSV( lift, up ) );
			foot->tilt = b3NLerp( foot->fromTilt, foot->targetTilt, smooth );
			float turn = nbWrapAngle( f->landHeading[k] - f->footHeading[k] );
			turn = nbSwingTurn( turn, f->swingTurnRate + k, until + timeStep, timeStep );
			f->footHeading[k] = nbWrapAngle( f->footHeading[k] + turn );

			// The heel comes down over the first half of the swing, and walking, the toes come up before the foot lands on
			// its heel. Walking, the heel first goes on up, the toes down by NB_GAIT_SWING_PITCH more a quarter into the
			// swing, so the knee bends as the foot leaves the ground, as people's does. Stepping backward, mirrored: the toes
			// first go on up and come down before the foot lands on them, and it rolls over its heel no farther than as it
			// lifted, and NB_FOOT_HEEL_MOST: going on rolling as fast as on the ground, it turned its toes up by 74 degrees.
			// Its toes come up only as far as it lands ahead of the hip, along its toes as it lands, see
			// NB_GAIT_HEEL_AHEAD: the frame goes on along its velocity until then, level, see below.
			float flat = b3MinFloat( 2.0f * along, 1.0f );
			float toesUp = b3ClampFloat( ( along - NB_GAIT_HEEL_FROM ) / ( 1.0f - NB_GAIT_HEEL_FROM ), 0.0f, 1.0f );
			float rest = ( 1.0f - f->swingStart[k] ) * footTime / ( f->swingRate[k] * f->swingHurry[k] );
			float goOn = flat < 1.0f ? along * ( 1.0f - flat ) * ( 1.0f - flat ) : 0.0f;
			float stepSide = f->stepBack[k] ? -1.0f : 1.0f;
			b3Vec3 on = b3Vec3_zero;
			float heelPart = 1.0f;
			if ( clipRoot == false )
			{
				on = nbFrameAhead( aimVelocity, swingAcceleration, aimWish, until, NULL );
				on = nbPlanAhead( f, &capsulePlan, until, plan, on, NULL );
				if ( f->stepBack[k] == false && f->legBack == false )
				{
					b3Vec3 lands = b3InvRotateVector( f->frame.q, b3Sub( b3SubPos( foot->target, f->frame.p ), on ) );
					b3Vec3 toes =
						b3InvRotateVector( f->frame.q, b3RotateVector( nbHeadingQuat( f->landHeading[k] ), b3Vec3_axisZ ) );
					float leads = toes.x * ( lands.x - hips[k].x ) + toes.z * ( lands.z - hips[k].z );
					float full = NB_GAIT_HEEL_AHEAD * b3Length( ahead );
					heelPart = full > 0.0f ? b3ClampFloat( leads / full, 0.0f, 1.0f ) : 1.0f;
				}
			}
			f->heelPart[k] = heelPart;
			f->footRoll[k] = f->swingRoll[k] * ( 1.0f - flat * flat * ( 3.0f - 2.0f * flat ) ) +
							 f->footRollRate[k] * rest * goOn +
							 stepSide * ( 1.0f - run ) * ease * NB_GAIT_SWING_PITCH * calm * bump -
							 stepSide * heelPart * heel * toesUp * toesUp * ( 3.0f - 2.0f * toesUp );
			if ( f->stepBack[k] )
			{
				f->footRoll[k] = b3MaxFloat( f->footRoll[k], b3MinFloat( f->swingRoll[k], -NB_FOOT_HEEL_MOST ) );
			}

			// With clips of its gait that move the root, it goes as they have it, and down as far as the legs need, see
			// nbLowerSteered
			if ( clipRoot == false )
			{
				// Walking, the root goes down before the foot lands, as far as the leg needs to reach where it lands from where
				// the hip is then: the frame goes on along its velocity and up or down its slope until then, and the pelvis turns
				// the hip as nbGaitPose turns it, with the legs and the swing of the pelvis, as running, see nbRunNeed. As far as
				// level ground needs, from NB_GAIT_DESCEND of the swing on, and what the foot lands lower, over the whole swing.
				// As the figure starts or stops running, this goes or comes with the spring of NB_RUN_TURN_LAG, as the running
				// descent comes or goes. Until round 107 walking took the hip as if the pelvis did not turn, while it turns with
				// the legs up to 70 degrees going sideways: the root went lower than the legs needed, going sideways at 1.6 m/s
				// 7.5 cm below standing, and came up from there once the foot landed, the pelvis over 10 mm in an update at 119
				// of 504 points of the changes of the gait.
				float late = b3ClampFloat( ( along - NB_GAIT_DESCEND ) / ( 1.0f - NB_GAIT_DESCEND ), 0.0f, 1.0f );
				float walking = 1.0f - springRun;
				float descend = walking * late * late * ( 3.0f - 2.0f * late );
				float early = walking * smooth;
				on = b3MulAdd( on, slope * speed * until, up );
				b3Vec3 landing = b3InvRotateVector( f->frame.q, b3Sub( b3SubPos( foot->target, f->frame.p ), on ) );
				if ( f->stepBack[k] == f->legBack )
				{
					landing = b3Add( landing, onHeel[k] );
				}
				else
				{
					// A swing that went the other way as it lifted lands on the other edge
					b3Vec3 onLand = b3Vec3_zero;
					b3Quat landRotation = clips[k].rotation;
					const nbBone* landThigh = f->bones + ( k == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight );
					nbRollFoot( landThigh + 2, ( f->stepBack[k] ? 1.0f : -1.0f ) * heel, 0.0f, &onLand, &landRotation );
					landing = b3Add( landing, onLand );
				}
				b3Vec3 hip = hips[k];
				float reach = reaches[k];
				float rate = f->gaitRate / cycle;
				float then = f->gaitPhase + until * rate;
				float turned = f->legTurn - f->gaitTurnSize * b3ComputeCosSin( 2.0f * B3_PI * ( then - timeStep * rate ) ).cosine;
				float vault = b3MaxFloat( f->root.p.y - levels[k], 0.0f );
				float deep = b3MaxFloat( nbRunNeed( f->root.p, hip, landing, turned, reach, 0.0f ), 0.0f );
				float running = 0.0f;
				if ( f->runTurn > 0.0f )
				{
					// Running, see NB_RUN_DESCEND: as far as the leg needs in each update until the foot lands, as late in its
					// swing as the foot is then, see NB_GAIT_DESCEND, below how low it runs all the time, as far as it runs, see
					// NB_RUN_TURN_LAG. The foot goes on its way, see nbSwingPath, the frame along its velocity and slope, and the
					// pelvis turns as nbGaitPose turns it, an update after the phase of the gait.
					float high = bob * b3ComputeCosSin( 4.0f * B3_PI * ( then - 0.5f * bobStance ) ).cosine;
					deep = b3MaxFloat( nbRunNeed( f->root.p, hip, landing, turned, reach, high ), 0.0f );
					float want = 0.0f;
					if ( until <= NB_RUN_DESCEND )
					{
						float soon = 1.0f - until / NB_RUN_DESCEND;
						float lack = nbRunNeed( f->root.p, hip, landing, turned, reach, high ) - steady;
						want = b3MaxFloat( soon * soon * ( 3.0f - 2.0f * soon ) * lack, 0.0f );
					}
					if ( until > timeStep )
					{
						float pace = f->swingRate[k] * f->swingHurry[k] / footTime;
						b3Vec3 left = b3SubPos( foot->target, f->swingBase[k] );
						b3Vec3 velocity = f->swingVelocity[k];
						b3Vec3 lag = b3MulSub( left, until, velocity );
						b3Vec3 bend = b3MulSV( 1.0f / ( until * until ), b3MulAdd( b3MulSV( 3.0f, lag ), until, velocity ) );
						b3Vec3 jerk =
							b3MulSV( -1.0f / ( until * until * until ), b3MulAdd( b3MulSV( 2.0f, lag ), until, velocity ) );
						b3Vec3 start = b3InvRotateVector( f->frame.q, b3SubPos( f->swingBase[k], f->frame.p ) );
						b3Vec3 drift =
							b3InvRotateVector( f->frame.q, b3Sub( velocity, b3MulAdd( aimVelocity, slope * speed, up ) ) );
						bend = b3InvRotateVector( f->frame.q, bend );
						jerk = b3InvRotateVector( f->frame.q, jerk );
						b3Vec3 lifted = b3InvRotateVector( f->frame.q, up );
						b3CosSin turnPhase = b3ComputeCosSin( 2.0f * B3_PI * ( f->gaitPhase - timeStep * rate ) );
						b3CosSin bobPhase = b3ComputeCosSin( 4.0f * B3_PI * ( f->gaitPhase - 0.5f * bobStance ) );
						b3CosSin turnStep = b3ComputeCosSin( 2.0f * B3_PI * timeStep * rate );
						b3CosSin bobStep = b3ComputeCosSin( 4.0f * B3_PI * timeStep * rate );
						for ( int m = 0;; ++m )
						{
							float time = (float)m * timeStep;
							if ( time >= until || time > NB_RUN_DESCEND )
							{
								break;
							}
							float alongThen = b3ClampFloat(
								( swing + time * pace - f->swingStart[k] ) / ( 1.0f - f->swingStart[k] ), 0.0f, 1.0f );
							float later =
								b3ClampFloat( ( alongThen - NB_GAIT_DESCEND ) / ( 1.0f - NB_GAIT_DESCEND ), 0.0f, 1.0f );
							if ( later > 0.0f )
							{
								b3Vec3 there = b3MulAdd( b3MulAdd( b3MulAdd( start, time, drift ), time * time, bend ),
														 time * time * time, jerk );
								float share = nbLiftShare( f->swingRun[k], springRun, alongThen );
								there = b3MulAdd( there, nbSwingLift( alongThen, share, height, higher, calm, f->swingBack[k] ),
												  lifted );
								float turned = f->legTurn - f->gaitTurnSize * turnPhase.cosine;
								float need = nbRunNeed( f->root.p, hip, there, turned, reach, bob * bobPhase.cosine ) - steady;
								float soon = 1.0f - time / NB_RUN_DESCEND;
								float weight = later * later * ( 3.0f - 2.0f * later ) * soon * soon * ( 3.0f - 2.0f * soon );
								want = b3MaxFloat( want, weight * need );
							}
							turnPhase = (b3CosSin){ turnPhase.cosine * turnStep.cosine - turnPhase.sine * turnStep.sine,
													turnPhase.sine * turnStep.cosine + turnPhase.cosine * turnStep.sine };
							bobPhase = (b3CosSin){ bobPhase.cosine * bobStep.cosine - bobPhase.sine * bobStep.sine,
												   bobPhase.sine * bobStep.cosine + bobPhase.cosine * bobStep.sine };
						}
					}
					running = f->runTurn * ( steady + want );
				}
				crouch = b3MaxFloat( crouch,
									 descend * b3MinFloat( vault, deep ) + early * b3MaxFloat( deep - vault, 0.0f ) + running );
			}
		}

		b3Vec3 gap = b3SubPos( nbClipAnkle( f, clips + k, foot ), foot->plant );
		gap = b3MulSub( gap, b3Dot( gap, up ), up );
		settled = settled && f->footSwings[k] == false && b3Length( gap ) < NB_GAIT_SLACK * scale &&
				  fabsf( nbWrapAngle( f->footHeading[k] - legHeading ) ) < NB_GAIT_TURN;
	}

	// Without a foothold to time a swing to, the gait goes back to its cadence, and so does each swing, and the cycle keeps
	// the pace of the swing that lands first, see NB_STAIR_RATE
	if ( f->footholdChosen[0] == false && f->footholdChosen[1] == false )
	{
		f->gaitRate += b3MinFloat( 1.0f, NB_GAIT_FOLLOW * timeStep ) * ( 1.0f - f->gaitRate );
	}
	float soonest = FLT_MAX;
	for ( int k = 0; k < 2; ++k )
	{
		if ( f->footholdChosen[k] == false )
		{
			f->swingRate[k] += b3MinFloat( 1.0f, NB_GAIT_FOLLOW * timeStep ) * ( 1.0f - f->swingRate[k] );
		}
		if ( f->footSwings[k] )
		{
			float footTime = nbSwingTime( f, k, swingTime, cycle, up );
			float until = ( 1.0f - f->swingAlong[k] ) * footTime / ( f->swingRate[k] * f->swingHurry[k] );
			if ( until < soonest )
			{
				soonest = until;
				f->gaitRate = f->swingRate[k];
			}
		}
	}

	// The root goes down as far as the legs need to reach a foot where it lands: running all the time and before a foot
	// lands as far as its leg needs until then, see NB_RUN_DESCEND, up and down with the steps, lowest in the middle of
	// the stance, walking only before a foot lands. Walking, it vaults over the leg it
	// stands on, as high as the clip has it and down as far as the legs need to reach both feet, see nbPlaceFeet. With
	// clips of its gait that move the root, it goes up and down as they have it instead of the bob and the vault.
	f->root.p.y -= f->gaitBlend * ( crouch + bob * b3ComputeCosSin( 4.0f * B3_PI * ( f->gaitPhase - 0.5f * bobStance ) ).cosine );
	f->gaitCrouch = crouch;
	f->gaitBob = bob;
	f->gaitStance = bobStance;
	f->gaitPhaseRate = f->gaitRate / cycle;

	// Still, with both feet where the clip puts them, it stands, and the clip takes the legs back, see NB_FOOT_FADE
	if ( settled && speed <= NB_GAIT_START && f->gaitSpeed < NB_GAIT_STOP )
	{
		f->gaitOn = false;
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Hits

// Remember a hit id, false if the figure took it before
static bool nbTakeHitId( nbFigure* f, uint32_t id )
{
	if ( id == 0 )
	{
		return true;
	}
	for ( int k = 0; k < f->hitMemoryCount; ++k )
	{
		if ( f->hitIds[k] == id )
		{
			return false;
		}
	}
	f->hitIds[f->hitNext] = id;
	f->hitNext = ( f->hitNext + 1 ) % NB_HIT_MEMORY;
	f->hitMemoryCount = f->hitMemoryCount < NB_HIT_MEMORY ? f->hitMemoryCount + 1 : NB_HIT_MEMORY;
	return true;
}

// Whether a bone carries another: it is that bone or one of its parents
static bool nbCarries( const nbFigure* f, int carrier, int i )
{
	while ( i > carrier )
	{
		i = f->bones[i].parent;
	}
	return i == carrier;
}

// Whether a bone of the humanoid is one of the spine, which share the flinch of the trunk, see NB_SPINE_COUNT
static bool nbIsSpine( const nbFigure* f, int i )
{
	return f->boneCount == nb_humanoidBoneCount && i >= nb_humanoidBelly && i < nb_humanoidBelly + NB_SPINE_COUNT;
}

// The part of a turn of the trunk, a rotation vector in the frame of the figure, that a bone of the spine of the humanoid
// takes: its shares of the tilt forward and back, of the tilt to the side and of the turn about the axis up the spine,
// see nbComputeTwist
static b3Vec3 nbSpinePart( const nbFigure* f, int i, b3Vec3 rotation )
{
	int k = i - nb_humanoidBelly;
	return (b3Vec3){ f->spineBend[k] * rotation.x, f->spineTwist[k] * rotation.y, f->spineSide[k] * rotation.z };
}

// A flinch of the trunk within its limits: the tilt up to flinchLimit and the turn about the axis up the spine of the
// humanoid up to the twist its spine allows, see nbComputeTwist. Other rigs turn their root up to flinchLimit in all.
static b3Vec3 nbClampFlinch( const nbFigure* f, b3Vec3 rotation )
{
	if ( f->boneCount != nb_humanoidBoneCount )
	{
		float angle = b3Length( rotation );
		return angle > f->def.flinchLimit ? b3MulSV( f->def.flinchLimit / angle, rotation ) : rotation;
	}

	float tilt = sqrtf( rotation.x * rotation.x + rotation.z * rotation.z );
	if ( tilt > f->def.flinchLimit )
	{
		float scale = f->def.flinchLimit / tilt;
		rotation.x *= scale;
		rotation.z *= scale;
	}
	rotation.y = b3ClampFloat( rotation.y, -f->twistLimit, f->twistLimit );
	return rotation;
}

// A trunk turns about its long axis more easily than it tilts about its root: the upper body of the humanoid, all the
// belly carries, has a rotational inertia about the joint of the belly, from the mass of the shape of every bone in its
// first pose, in the frame of the figure, 0.8 kg m^2 about the axis up the spine and 5 about the axes across it. A hit
// turns it that much faster about the axis up the spine, against muscles as stiff about every axis it swings out the
// square root of that farther and back as much faster, see nbFlinch. The joints of the spine share a twist after their
// twist ranges, the lumbar spine the least, and together they allow the twist that brings the first of them to its
// limit. Other rigs turn about their root with the twist as far as the tilt. They share a tilt forward and back and one
// to the side after their swing ranges the same way, so a light hit bends the humanoid most in its belly, as a person
// bends most in the lumbar spine, see nbMakeHumanoidBones.
static void nbComputeTwist( nbFigure* f )
{
	f->twistGain = 1.0f;
	f->twistLimit = f->def.flinchLimit;
	for ( int k = 0; k < NB_SPINE_COUNT; ++k )
	{
		f->spineTwist[k] = 1.0f / (float)NB_SPINE_COUNT;
		f->spineBend[k] = 1.0f / (float)NB_SPINE_COUNT;
		f->spineSide[k] = 1.0f / (float)NB_SPINE_COUNT;
	}
	if ( f->boneCount != nb_humanoidBoneCount )
	{
		return;
	}

#if NB_SWING_LIMITS
	float bend[NB_SPINE_COUNT];
	float side[NB_SPINE_COUNT];
	float bendSum = 0.0f;
	float sideSum = 0.0f;
	for ( int k = 0; k < NB_SPINE_COUNT; ++k )
	{
		b3JointId jointId = f->bones[nb_humanoidBelly + k].jointId;
		float lowerX, upperX, lowerY, upperY;
		b3SphericalJoint_GetSwingLimits( jointId, &lowerX, &upperX, &lowerY, &upperY );
		bend[k] = b3MaxFloat( upperX - lowerX, 0.0f );
		side[k] = b3MaxFloat( upperY - lowerY, 0.0f );
		bendSum += bend[k];
		sideSum += side[k];
	}
	for ( int k = 0; k < NB_SPINE_COUNT && bendSum > 0.0f && sideSum > 0.0f; ++k )
	{
		f->spineBend[k] = bend[k] / bendSum;
		f->spineSide[k] = side[k] / sideSum;
	}
#endif

	b3Pos pivot = f->targets[nb_humanoidBelly].p;
	b3Matrix3 inertia = { b3Vec3_zero, b3Vec3_zero, b3Vec3_zero };
	for ( int i = nb_humanoidBelly; i < f->boneCount; ++i )
	{
		b3ShapeId shapeId;
		if ( nbCarries( f, nb_humanoidBelly, i ) == false || b3Body_GetShapes( f->bones[i].bodyId, &shapeId, 1 ) < 1 )
		{
			continue;
		}
		b3MassData data = b3Shape_ComputeMassData( shapeId );
		b3Matrix3 rotation = b3MakeMatrixFromQuat( b3InvMulQuat( f->frame.q, f->targets[i].q ) );
		b3Vec3 offset =
			b3InvRotateVector( f->frame.q, b3SubPos( b3TransformWorldPoint( f->targets[i], data.center ), pivot ) );
		inertia = b3AddMM( inertia, b3MulMM( rotation, b3MulMM( data.inertia, b3Transpose( rotation ) ) ) );
		inertia = b3AddMM( inertia, b3Steiner( data.mass, offset ) );
	}
	float twist = inertia.cy.y;
	float tilt = 0.5f * ( inertia.cx.x + inertia.cz.z );
	f->twistGain = twist > 0.0f ? sqrtf( b3MaxFloat( 1.0f, tilt / twist ) ) : 1.0f;

	float lower[NB_SPINE_COUNT];
	float upper[NB_SPINE_COUNT];
	float sum = 0.0f;
	for ( int k = 0; k < NB_SPINE_COUNT; ++k )
	{
		const nbBone* bone = f->bones + nb_humanoidBelly + k;
		lower[k] = 2.0f * b3Atan2( bone->lowerHalf.sine, bone->lowerHalf.cosine );
		upper[k] = 2.0f * b3Atan2( bone->upperHalf.sine, bone->upperHalf.cosine );
		sum += b3MaxFloat( upper[k] - lower[k], 0.0f );
	}
	if ( sum <= 0.0f )
	{
		return;
	}
	f->twistLimit = B3_HUGE;
	for ( int k = 0; k < NB_SPINE_COUNT; ++k )
	{
		f->spineTwist[k] = b3MaxFloat( upper[k] - lower[k], 0.0f ) / sum;
		if ( f->spineTwist[k] > 0.0f )
		{
			f->twistLimit = b3MinFloat( f->twistLimit, b3MinFloat( upper[k], -lower[k] ) / f->spineTwist[k] );
		}
	}
	f->twistLimit = b3MaxFloat( f->twistLimit, 0.0f );
}

// The bones a bone carries as one rigid body about its joint, in the frame of the figure: their mass, their center of
// mass from the joint and their rotational inertia about it, from their masses at their centers with a radius of
// gyration of 5 cm each
static float nbCarriedBody( const nbFigure* f, int b, float* mass, b3Vec3* center )
{
	b3Pos pivot = f->targets[b].p;
	float sum = 0.0f;
	float inertia = 0.0f;
	b3Vec3 moment = b3Vec3_zero;
	for ( int i = b; i < f->boneCount; ++i )
	{
		if ( nbCarries( f, b, i ) )
		{
			const nbBone* bone = f->bones + i;
			b3Vec3 local = bone->shape == nb_capsuleBone ? b3MulSV( 0.5f, b3Add( bone->start, bone->end ) ) : bone->center;
			b3Vec3 offset = b3InvRotateVector( f->frame.q, b3SubPos( b3TransformWorldPoint( f->targets[i], local ), pivot ) );
			sum += bone->mass;
			moment = b3MulAdd( moment, bone->mass, offset );
			inertia += bone->mass * ( b3LengthSquared( offset ) + 0.0025f );
		}
	}
	*mass = sum;
	*center = b3MulSV( 1.0f / sum, moment );
	return inertia;
}

// How fast an impulse at a point turns the bones a bone carries about its joint while the joint holds, and the momentum
// they gain for that, which the joint does not pass on, in the frame of the figure
static b3Vec3 nbSpinCarried( const nbFigure* f, int b, b3Vec3 impulse, b3Pos point, b3Vec3* momentum )
{
	float mass;
	b3Vec3 center;
	float inertia = nbCarriedBody( f, b, &mass, &center );
	b3Vec3 lever = b3InvRotateVector( f->frame.q, b3SubPos( point, f->targets[b].p ) );
	b3Vec3 spin = b3MulSV( 1.0f / inertia, b3Cross( lever, b3InvRotateVector( f->frame.q, impulse ) ) );
	*momentum = b3MulSV( mass, b3Cross( spin, center ) );
	return spin;
}

// How the bones a bone carries turn in the frame of the figure when its joint starts to move. What hangs beside the body,
// an arm, hangs free on its joint as a pendulum, its center of mass stays behind and it does not slide into the body as
// the trunk tilts under it. Above or below the root, the head keeps its turn, the neck holds it against its joint.
static b3Vec3 nbLagCarried( const nbFigure* f, int b, b3Vec3 pivotVelocity )
{
	if ( f->bones[b].hangs == false )
	{
		return b3Vec3_zero;
	}
	float mass;
	b3Vec3 center;
	float inertia = nbCarriedBody( f, b, &mass, &center );
	return b3MulSV( -mass / inertia, b3Cross( center, pivotVelocity ) );
}

// A standing figure flinches from an impulse at a point of a bone, after the roadmap of the game, 26.4 and 26.5. The
// trunk turns about its root away from the hit. A hit on a bone with a spring of its own, see nbBoneDef::flinch, or on a
// bone such a bone carries turns the bones of that chain about their joints, each flinch times as fast as the hit would
// turn what it carries while its joint held, the head of the humanoid from a hit on its face, an arm from one on its
// hand, and the trunk turns from the impulse the joint at the top of the chain passes on, what the chain does not take.
// The other bones with a spring stay where they were while the trunk and the chain turn, and swing after them.
static void nbFlinch( nbFigure* f, int hitBone, b3Vec3 impulse, b3Pos point )
{
	// The top of the chain of bones with a spring that carry the hit bone, or the trunk
	int top = 0;
	for ( int i = f->hasSwings ? hitBone : 0; i > 0; i = f->bones[i].parent )
	{
		top = f->bones[i].flinchOmega > 0.0f ? i : top;
	}
	b3Vec3 chainImpulse = impulse;
	b3Vec3 trunkImpulse = impulse;
	b3Pos trunkPoint = point;
	if ( top > 0 )
	{
		// A chain beside the body, an arm that hangs, does not swing into it: the part of the hit that pushes it toward the
		// axis up through the root, along the ground, the body behind it takes. The head above the root swings freely.
		if ( f->bones[top].hangs )
		{
			float mass;
			b3Vec3 center;
			nbCarriedBody( f, top, &mass, &center );
			b3Vec3 aside = b3Add( b3InvRotateVector( f->frame.q, b3SubPos( f->targets[top].p, f->targets[0].p ) ), center );
			aside.y = 0.0f;
			aside = b3RotateVector( f->frame.q, b3Normalize( aside ) );
			float inward = -b3Dot( impulse, aside );
			chainImpulse = inward > 0.0f ? b3MulAdd( impulse, inward, aside ) : impulse;
		}

		// The joint at the top passes on the impulse less the momentum the chain gains as loose as it swings, at most the
		// impulse itself
		b3Vec3 momentum;
		nbSpinCarried( f, top, chainImpulse, point, &momentum );
		trunkImpulse = b3MulSub( impulse, f->bones[top].flinch, b3RotateVector( f->frame.q, momentum ) );
		float passed = b3Length( trunkImpulse );
		float limit = b3Length( impulse );
		trunkImpulse = passed > limit ? b3MulSV( limit / passed, trunkImpulse ) : trunkImpulse;
		trunkPoint = f->targets[top].p;
	}

	// A critically damped spring kicked with a rate swings out the farthest after one over its angular frequency, by the
	// rate over e times that frequency: the kick that tilts the trunk by the flinch scale times the impulse it takes. It
	// turns the upper body of the humanoid twistGain times as far about the axis up its spine and twistGain times as fast
	// back, see nbComputeTwist.
	b3Vec3 trunkKick = b3Vec3_zero;
	b3Vec3 turn = b3InvRotateVector( f->frame.q, b3Cross( b3SubPos( trunkPoint, f->targets[0].p ), trunkImpulse ) );
	float length = b3Length( turn );
	if ( length >= 1.0e-6f )
	{
		float angle = f->def.flinchScale * b3Length( trunkImpulse ) / length;
		b3Vec3 rotation = { angle * turn.x, f->twistGain * angle * turn.y, angle * turn.z };
		rotation = nbClampFlinch( f, rotation );
		float omega = 2.7182818f * 2.0f * B3_PI * b3MaxFloat( f->def.flinchHertz, 0.1f );
		trunkKick = (b3Vec3){ omega * rotation.x, f->twistGain * omega * rotation.y, omega * rotation.z };
		f->flinchRate = b3Add( f->flinchRate, trunkKick );
	}
	if ( f->hasSwings == false )
	{
		return;
	}

	// How fast every bone turns from the hit in the frame of the figure and how fast its joint moves, parents first: a bone
	// without a spring with its parent, the spine of the humanoid by its share of the trunk. A bone with a spring keeps
	// its center of mass where it was while its joint moves, see nbLagCarried, and a bone of the chain turns as the hit
	// turns what it carries besides. Its spring takes the difference to its parent, and what its parent gained in turning
	// and its joint in moving counts as carried, see nbSwingBones.
	f->bones[0].turnRate = f->boneCount == nb_humanoidBoneCount ? b3Vec3_zero : trunkKick;
	f->bones[0].pivotVelocity = b3Vec3_zero;
	for ( int b = 1; b < f->boneCount; ++b )
	{
		nbBone* bone = f->bones + b;
		const nbBone* parent = f->bones + bone->parent;
		b3Vec3 lever = b3InvRotateVector( f->frame.q, b3SubPos( f->targets[b].p, f->targets[bone->parent].p ) );
		bone->pivotVelocity = b3Add( parent->pivotVelocity, b3Cross( parent->turnRate, lever ) );
		if ( bone->flinchOmega <= 0.0f )
		{
			bone->turnRate = nbIsSpine( f, b ) ? b3Add( parent->turnRate, nbSpinePart( f, b, trunkKick ) ) : parent->turnRate;
			continue;
		}

		b3Vec3 kick = nbLagCarried( f, b, bone->pivotVelocity );
		if ( top > 0 && nbCarries( f, b, hitBone ) )
		{
			b3Vec3 momentum;
			kick = b3MulAdd( kick, bone->flinch, nbSpinCarried( f, b, chainImpulse, point, &momentum ) );
		}
		bone->swingRate = b3Add( bone->swingRate, b3Sub( kick, parent->turnRate ) );
		bone->carried = b3Add( bone->carried, parent->turnRate );
		bone->carriedVelocity = b3Add( bone->carriedVelocity, bone->pivotVelocity );
		bone->turnRate = kick;
	}
	f->swinging = true;
}

// The way from a point on a bone through the joints of the skeleton to the center of mass of another bone: up from the bone
// it starts on to the bone both hang from, and down to the other, from joint to joint. The joint of a bone with its parent
// is the origin of its body. On the bone it starts on, the way goes straight to its center.
static float nbSkeletonDistance( const nbFigure* f, int from, b3Pos point, int to )
{
	b3Pos center = b3Body_GetWorldCenter( f->bones[to].bodyId );
	if ( from == to )
	{
		return b3Length( b3SubPos( center, point ) );
	}

	// The bone both hang from
	int fromDepth = 0;
	for ( int b = from; b > 0; b = f->bones[b].parent )
	{
		fromDepth += 1;
	}
	int toDepth = 0;
	for ( int b = to; b > 0; b = f->bones[b].parent )
	{
		toDepth += 1;
	}
	int a = from;
	int b = to;
	for ( ; fromDepth > toDepth; fromDepth -= 1 )
	{
		a = f->bones[a].parent;
	}
	for ( ; toDepth > fromDepth; toDepth -= 1 )
	{
		b = f->bones[b].parent;
	}
	while ( a != b )
	{
		a = f->bones[a].parent;
		b = f->bones[b].parent;
	}
	int common = a;

	// Up from the point to the joint below the common bone
	float length = 0.0f;
	b3Pos at = point;
	for ( int k = from; k != common; k = f->bones[k].parent )
	{
		b3Pos joint = b3Body_GetPosition( f->bones[k].bodyId );
		length += b3Length( b3SubPos( joint, at ) );
		at = joint;
	}
	if ( to == common )
	{
		return length + b3Length( b3SubPos( center, at ) );
	}

	// Down to the bone, gathered from it upward
	b3Pos below = b3Body_GetPosition( f->bones[to].bodyId );
	float down = b3Length( b3SubPos( center, below ) );
	for ( int k = f->bones[to].parent; k != common; k = f->bones[k].parent )
	{
		b3Pos joint = b3Body_GetPosition( f->bones[k].bodyId );
		down += b3Length( b3SubPos( below, joint ) );
		below = joint;
	}
	return length + b3Length( b3SubPos( below, at ) ) + down;
}

// The upper body of the humanoid, all the belly carries, as one body that turns about an axis through its center of mass:
// how fast each of its bones moves and turns with it, from the impulse that turns it, added to its bones
static void nbTurnUpperBody( nbFigure* f, b3Vec3 axis, b3Pos center, float omega )
{
	for ( int i = nb_humanoidBelly; i < f->boneCount; ++i )
	{
		if ( nbCarries( f, nb_humanoidBelly, i ) )
		{
			b3BodyId bodyId = f->bones[i].bodyId;
			b3Matrix3 rotation = b3MakeMatrixFromQuat( b3Body_GetRotation( bodyId ) );
			b3Matrix3 inertia =
				b3MulMM( rotation, b3MulMM( b3Body_GetLocalRotationalInertia( bodyId ), b3Transpose( rotation ) ) );
			b3Vec3 lever = b3SubPos( b3Body_GetWorldCenter( bodyId ), center );
			b3Body_ApplyLinearImpulseToCenter( bodyId, b3MulSV( b3Body_GetMass( bodyId ) * omega, b3Cross( axis, lever ) ),
											   true );
			b3Body_ApplyAngularImpulse( bodyId, b3MulSV( omega, b3MulMV( inertia, axis ) ), true );
		}
	}
}

// A hit on the trunk of the humanoid off its middle turns it about its long axis, see F50 of the roadmap. The spread puts
// every share of the hit at the center of mass of its bone, as if it came through them: the moment of the hit about the
// axis up the spine that this misses, the moment about the point where the spread puts the hit, the upper body takes as
// one body turning about that axis through its center of mass, the bones of the arms, the neck and the head with it, with
// no momentum along the way. The other axes stay as the spread leaves them: with the moment of the hit about them the
// trunk tipped over the pelvis and the figure caught itself a tenth less often, see F46. A hit on a limb or the head
// turns the upper body through its joint, and so does the spread.
static void nbTwistTrunk( nbFigure* f, int hitBone, b3Vec3 impulse, b3Vec3 spread )
{
	if ( nbIsSpine( f, hitBone ) == false )
	{
		return;
	}

	b3Pos low = b3Body_GetPosition( f->bones[nb_humanoidBelly].bodyId );
	b3Vec3 axis = b3Normalize( b3SubPos( b3Body_GetPosition( f->bones[nb_humanoidNeck].bodyId ), low ) );
	float mass = 0.0f;
	b3Vec3 moment = b3Vec3_zero;
	for ( int i = nb_humanoidBelly; i < f->boneCount; ++i )
	{
		if ( nbCarries( f, nb_humanoidBelly, i ) )
		{
			float boneMass = b3Body_GetMass( f->bones[i].bodyId );
			mass += boneMass;
			moment = b3MulAdd( moment, boneMass, b3SubPos( b3Body_GetWorldCenter( f->bones[i].bodyId ), low ) );
		}
	}
	if ( mass <= 0.0f )
	{
		return;
	}

	// The rotational inertia of the upper body about the axis through its center of mass
	b3Pos center = b3OffsetPos( low, b3MulSV( 1.0f / mass, moment ) );
	float inertia = 0.0f;
	for ( int i = nb_humanoidBelly; i < f->boneCount; ++i )
	{
		if ( nbCarries( f, nb_humanoidBelly, i ) )
		{
			b3BodyId bodyId = f->bones[i].bodyId;
			b3Matrix3 rotation = b3MakeMatrixFromQuat( b3Body_GetRotation( bodyId ) );
			b3Vec3 local = b3MulMV( b3Transpose( rotation ), axis );
			b3Vec3 lever = b3SubPos( b3Body_GetWorldCenter( bodyId ), center );
			float along = b3Dot( lever, axis );
			inertia += b3Dot( local, b3MulMV( b3Body_GetLocalRotationalInertia( bodyId ), local ) ) +
					   b3Body_GetMass( bodyId ) * ( b3Dot( lever, lever ) - along * along );
		}
	}
	if ( inertia <= 0.0f )
	{
		return;
	}

	// spread is where the shares put the hit, from the point: the moment the hit misses about it is -spread x impulse
	float missing = -b3Dot( b3Cross( spread, impulse ), axis );
	nbTurnUpperBody( f, axis, center, missing / inertia );
}

// A hit spread over the dynamic bones of a figure, as Overgrowth's RiggedObject::ApplyForceToRagdoll spreads it, but along
// the skeleton: every bone takes a part as large as its mass over the square of its way from the point through the joints
// plus 0.01 square meters, see nbSkeletonDistance, at its center of mass. A bone that took all of it would whip about its
// joints, a hard hit on the chest folds the trunk at the hips. Measured straight, as in Overgrowth, a forearm that hangs
// beside the hip took a fifth of a hit on it and the pelvis as much, and a kick on one shin pushed the other one too.
// A hit on the trunk also turns it about its long axis, see nbTwistTrunk.
static void nbSpreadHit( nbFigure* f, int hitBone, b3Vec3 impulse, b3Pos point )
{
	float total = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		float distance = nbSkeletonDistance( f, hitBone, point, i );
		total += b3Body_GetMass( f->bones[i].bodyId ) / ( distance * distance + 0.01f );
	}
	if ( total <= 0.0f )
	{
		return;
	}

	// The two parts of the chest of the humanoid are one rib cage, which the ribs and the breastbone hold together: they
	// take their parts of the hit together, shared by their masses, so they move off as one, as the chest of one part did
	// before round 37. Each at its own center, the upper one, at the height of a hit on the breastbone, moved off a third
	// faster than the chest of one part had, the trunk tipped about the pelvis, and the figure caught itself less often.
	bool cage = f->boneCount == nb_humanoidBoneCount;
	float cagePart = 0.0f;
	float cageMass = 0.0f;
	for ( int i = nb_humanoidLowerChest; cage && i <= nb_humanoidChest; ++i )
	{
		float distance = nbSkeletonDistance( f, hitBone, point, i );
		float mass = b3Body_GetMass( f->bones[i].bodyId );
		cagePart += mass / ( distance * distance + 0.01f ) / total;
		cageMass += mass;
	}

	b3Vec3 spread = b3Vec3_zero;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3BodyId bodyId = f->bones[i].bodyId;
		float distance = nbSkeletonDistance( f, hitBone, point, i );
		float part = b3Body_GetMass( bodyId ) / ( distance * distance + 0.01f ) / total;
		if ( cage && ( i == nb_humanoidLowerChest || i == nb_humanoidChest ) )
		{
			part = cagePart * b3Body_GetMass( bodyId ) / cageMass;
		}
		b3Body_ApplyLinearImpulseToCenter( bodyId, b3MulSV( part, impulse ), true );
		spread = b3MulAdd( spread, part, b3SubPos( b3Body_GetWorldCenter( bodyId ), point ) );
	}

	nbTwistTrunk( f, hitBone, impulse, spread );
	nbReadVelocities( f );
}

// A hit on the dynamic bones of a figure, spread over them from the bone it hit, see nbSpreadHit. The whole impulse on the
// bone it hit, as on the branch ragdoll-11, threw the trunk of a figure that had stood animated a moment before so far that
// it fell from 80 N s on its chest and 60 N s on its back, which ragdoll-11 caught with steps. A hard hit on a joint that
// gives way softens it, see nbSoftenJoints.
static void nbHitBones( nbFigure* f, int bone, b3Vec3 impulse, b3Pos point )
{
	nbSpreadHit( f, bone, impulse, point );
	nbSoftenJoints( f, bone, impulse );
}

// A hit on a figure, from the program, a blast or debris
static void nbTakeHit( nbWorld* world, nbFigure* f, int bone, b3Vec3 impulse, b3Pos point, bool fromDebris )
{
	f->hitCount += 1;
	float magnitude = b3Length( impulse );

	// Debris that hits dynamic bones pushed them in the step already
	bool dynamic = f->state == nb_figureRagdoll || f->state == nb_figureBalancing;
	if ( dynamic && fromDebris )
	{
		return;
	}

	// A figure that catches itself takes the hit as it is and catches that too, if it can
	if ( f->state == nb_figureBalancing )
	{
		nbHitBones( f, bone, impulse, point );
		return;
	}

	// A figure that gets up breaks off from much less, see the roadmap 26.15
	float threshold = f->def.knockdownImpulse;
	if ( f->state == nb_figureGettingUp )
	{
		threshold *= NB_ABORT_PART;
	}

	if ( f->state != nb_figureRagdoll && magnitude < threshold )
	{
		if ( f->state == nb_figureAnimated )
		{
			nbFlinch( f, bone, impulse, point );

			// A push it cannot catch over its feet makes it catch itself after all
			if ( nbPush( world, f, impulse ) == false )
			{
				nbBeginBalancing( world, f );
				nbHitBones( f, bone, impulse, point );
			}
		}
		return;
	}

	// A standing figure catches itself, if it can, one that gets up falls, a ragdoll is slack a moment
	if ( f->state == nb_figureAnimated )
	{
		nbBeginBalancing( world, f );
	}
	else
	{
		nbFall( world, f, f->state == nb_figureRagdoll );
	}
	nbHitBones( f, bone, impulse, point );
}

// The impulse a blast gives a bone, as b3World_Explode gives it: the impulse per area times the area the bone turns to the
// blast, falling off from the radius to twice the radius. The area of a capsule seen from the side.
static b3Vec3 nbBlastImpulse( const nbFigure* f, int index, const nbImpactDef* def )
{
	const nbBone* bone = f->bones + index;
	b3WorldTransform transform = b3Body_GetTransform( bone->bodyId );
	b3Vec3 center = bone->shape == nb_capsuleBone ? b3MulSV( 0.5f, b3Add( bone->start, bone->end ) ) : bone->center;
	b3Vec3 offset = b3SubPos( b3TransformWorldPoint( transform, center ), def->point );
	float distance = b3Length( offset );
	float size;
	float area;
	if ( bone->shape == nb_capsuleBone )
	{
		float length = b3Length( b3Sub( bone->end, bone->start ) );
		size = bone->radius;
		area = 2.0f * bone->radius * length + B3_PI * bone->radius * bone->radius;
	}
	else
	{
		b3Vec3 h = bone->halfExtents;
		size = b3MinFloat( h.x, b3MinFloat( h.y, h.z ) );
		area = 4.0f * b3MaxFloat( h.x * h.y, b3MaxFloat( h.y * h.z, h.x * h.z ) );
	}

	float radius = def->radius;
	float gap = b3MaxFloat( distance - size, 0.0f );
	if ( gap > 2.0f * radius || distance < 1.0e-6f )
	{
		return b3Vec3_zero;
	}
	float scale = gap > radius ? b3ClampFloat( ( 2.0f * radius - gap ) / radius, 0.0f, 1.0f ) : 1.0f;
	return b3MulSV( def->explosionImpulse * area * scale / distance, offset );
}

void nbBlastFigures( nbWorld* world, const nbImpactDef* def )
{
	if ( def->explosionImpulse <= 0.0f || world->figures.count == 0 )
	{
		return;
	}

	for ( int index = 0; index < world->figures.count; ++index )
	{
		nbFigure* f = world->figures.data + index;
		if ( f->isFree || f->state == nb_figureRagdoll || f->state == nb_figureBalancing )
		{
			continue;
		}

		// The blast on the whole figure and its strongest bone
		b3Vec3 total = b3Vec3_zero;
		int strongest = 0;
		float strongestImpulse = 0.0f;
		for ( int i = 0; i < f->boneCount; ++i )
		{
			b3Vec3 impulse = nbBlastImpulse( f, i, def );
			total = b3Add( total, impulse );
			float magnitude = b3Length( impulse );
			if ( magnitude > strongestImpulse )
			{
				strongestImpulse = magnitude;
				strongest = i;
			}
		}

		float magnitude = b3Length( total );
		if ( magnitude == 0.0f )
		{
			continue;
		}

		f->hitCount += 1;
		if ( magnitude < f->def.knockdownImpulse )
		{
			// The blast does not reach kinematic bones, the figure flinches from it at its strongest bone and steps to catch
			// it, see nbPush
			b3Pos point = b3Body_GetWorldCenter( f->bones[strongest].bodyId );
			nbFlinch( f, strongest, total, point );
			if ( nbPush( world, f, total ) )
			{
				continue;
			}
		}

		// Dynamic before the blast, so it throws the bones itself, once. A standing figure catches itself if it can, one
		// that gets up falls.
		if ( f->state == nb_figureAnimated )
		{
			nbBeginBalancing( world, f );
		}
		else
		{
			nbFall( world, f, true );
		}
	}
}

void nbReadFigureVelocities( nbWorld* world )
{
	for ( int index = 0; index < world->figures.count; ++index )
	{
		nbFigure* f = world->figures.data + index;
		if ( f->isFree == false && ( f->state == nb_figureRagdoll || f->state == nb_figureBalancing || f->following ) )
		{
			nbReadVelocities( f );
		}
	}
}

// Box3D stopped the bones against the chunks as if they were the world, and the joints passed it on to the whole body.
// Where the chunks broke away, the wall gave way under the figure: every bone gets back up to the share of its own speed
// toward the wall from before the step, so the figure goes on through the hole in the pose it flew in. The joints may
// have passed momentum from one bone to another in the step, a head that whips ahead of the chest the wall stopped, so
// all bones together get back no more than the share of the momentum of the whole figure toward the wall, bones that
// moved away from it counted against it. It never gains momentum or energy.
float nbPassFigureThrough( nbWorld* world, int index, b3Vec3 normal, float keep )
{
	nbFigure* f = world->figures.data + index;
	float before = 0.0f;
	float now = 0.0f;
	float added = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		float approach = -b3Dot( bone->stepVelocity, normal );
		float toward = -b3Dot( b3Body_GetLinearVelocity( bone->bodyId ), normal );
		before += bone->mass * approach;
		now += bone->mass * toward;
		if ( approach > 0.0f && toward < keep * approach )
		{
			added += bone->mass * ( keep * approach - toward );
		}
	}
	if ( before <= 0.0f )
	{
		return 0.0f;
	}

	float room = keep * before - now;
	float scale = added > 0.0f ? b3ClampFloat( room / added, 0.0f, 1.0f ) : 0.0f;
	if ( scale > 0.0f )
	{
		for ( int i = 0; i < f->boneCount; ++i )
		{
			const nbBone* bone = f->bones + i;
			b3Vec3 velocity = b3Body_GetLinearVelocity( bone->bodyId );
			float approach = -b3Dot( bone->stepVelocity, normal );
			float toward = -b3Dot( velocity, normal );
			if ( approach > 0.0f && toward < keep * approach )
			{
				float gain = scale * ( keep * approach - toward );
				b3Body_SetLinearVelocity( bone->bodyId, b3MulSub( velocity, gain, normal ) );
			}
		}
	}
	return b3ClampFloat( ( now + scale * added ) / before, 0.0f, 1.0f );
}

// The strongest hit of debris on every standing figure and every figure that gets up in the last step, from the hit
// events of Box3D: a kinematic bone is as heavy as the world, so the debris bounced off it with about its momentum toward
// the bone
// The normal impulse that the contact of two shapes gave a body in the last step
static float nbContactImpulse( b3BodyId bodyId, b3ShapeId shapeA, b3ShapeId shapeB )
{
	b3ContactData contacts[NB_CONTACT_CAPACITY];
	int count = b3Body_GetContactData( bodyId, contacts, NB_CONTACT_CAPACITY );
	for ( int c = 0; c < count; ++c )
	{
		const b3ContactData* contact = contacts + c;
		bool same = ( B3_ID_EQUALS( contact->shapeIdA, shapeA ) && B3_ID_EQUALS( contact->shapeIdB, shapeB ) ) ||
					( B3_ID_EQUALS( contact->shapeIdA, shapeB ) && B3_ID_EQUALS( contact->shapeIdB, shapeA ) );
		if ( same == false )
		{
			continue;
		}

		float impulse = 0.0f;
		for ( int m = 0; m < contact->manifoldCount; ++m )
		{
			const b3Manifold* manifold = contact->manifolds + m;
			for ( int p = 0; p < manifold->pointCount; ++p )
			{
				impulse += manifold->points[p].totalNormalImpulse;
			}
		}
		return impulse;
	}
	return 0.0f;
}

static void nbCollectDebrisHits( nbWorld* world )
{
	for ( int index = 0; index < world->figures.count; ++index )
	{
		world->figures.data[index].debrisImpulse = 0.0f;
	}

	b3ContactEvents events = b3World_GetContactEvents( world->physicsWorld );
	for ( int i = 0; i < events.hitCount; ++i )
	{
		const b3ContactHitEvent* event = events.hitEvents + i;
		if ( b3Shape_IsValid( event->shapeIdA ) == false || b3Shape_IsValid( event->shapeIdB ) == false )
		{
			continue;
		}

		b3BodyId bodyA = b3Shape_GetBody( event->shapeIdA );
		b3BodyId bodyB = b3Shape_GetBody( event->shapeIdB );
		for ( int side = 0; side < 2; ++side )
		{
			int bone;
			int index = nbFigureOfBody( world, side == 0 ? bodyA : bodyB, &bone );
			if ( index == NB_NULL_INDEX )
			{
				continue;
			}

			nbFigure* f = world->figures.data + index;
			b3BodyId boneBody = side == 0 ? bodyA : bodyB;
			b3BodyId other = side == 0 ? bodyB : bodyA;
			b3BodyType otherType = b3Body_GetType( other );
			if ( f->state == nb_figureRagdoll || f->state == nb_figureBalancing ||
				 ( f->following ? otherType == b3_staticBody : otherType != b3_dynamicBody ) ||
				 nbFigureOfBody( world, other, NULL ) == index )
			{
				continue;
			}

			// The normal points from A to B, the other body pushes the bone away from it. What the bone moved toward it
			// itself does not count: a figure that gets up next to a ragdoll or walks into rubble at rest pushes it aside.
			// The dynamic bones of a figure that follows its clip took the hit: the impulse of the contact in the step,
			// with the mass and the drive behind what hit them.
			float impulse;
			if ( f->following )
			{
				impulse = nbContactImpulse( boneBody, event->shapeIdA, event->shapeIdB );
			}
			else
			{
				b3Vec3 toOther = side == 0 ? event->normal : b3Neg( event->normal );
				b3Vec3 boneVelocity = b3Body_GetWorldPointVelocity( boneBody, event->point );
				float approach = event->approachSpeed - b3Dot( boneVelocity, toOther );
				impulse = b3Body_GetMass( other ) * approach;
			}
			if ( impulse > f->debrisImpulse )
			{
				f->debrisImpulse = impulse;
				f->debrisDirection = side == 0 ? b3Neg( event->normal ) : event->normal;
				f->debrisPoint = event->point;
				f->debrisBone = bone;
			}
		}
	}
}

// How hard dynamic bodies of others press down on the bones of a figure, in newtons: the normal impulses of their contacts
// in the last step over its time, of those that push down. The load on the figure, not what its bones push aside. With
// push, for a figure that stands or catches itself, it also gives the strongest push on one bone: sideways, of the
// contacts that push within 45 degrees of level, and on an arm of a humanoid that can be pushed from any side. Trunk,
// head and legs carry a load to the ground, an arm gives way: a stick pressed into the elbow pushes the round ends of
// upper arm and forearm up and down, and is no load. With arms, the strongest push on a bone of each arm. With fighters,
// for a figure that follows its clip near others, the level push of other figures on all its bones together, in newtons.
static float nbPressingForce( const nbWorld* world, int index, const nbFigure* f, float timeStep, b3Vec3 up, float* push,
							  float* arms, b3Vec3* fighters )
{
	b3ContactData contacts[NB_CONTACT_CAPACITY];
	float force = 0.0f;
	float strongest = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Vec3 bonePush = b3Vec3_zero;
		bool arm =
			push != NULL && f->def.pushForce > 0.0f && f->canBalance && i >= nb_humanoidUpperArmLeft && i <= nb_humanoidHandRight;
		b3BodyId bodyId = f->bones[i].bodyId;
		int count = b3Body_GetContactData( bodyId, contacts, NB_CONTACT_CAPACITY );
		for ( int c = 0; c < count; ++c )
		{
			const b3ContactData* contact = contacts + c;
			b3BodyId bodyA = b3Shape_GetBody( contact->shapeIdA );
			bool boneIsA = bodyA.index1 == bodyId.index1;
			b3BodyId other = boneIsA ? b3Shape_GetBody( contact->shapeIdB ) : bodyA;
			int otherFigure = b3Body_GetType( other ) == b3_dynamicBody ? nbFigureOfBody( world, other, NULL ) : index;
			if ( otherFigure == index )
			{
				continue;
			}

			// Fighters press on each other in a block or a clinch: what a figure with an engage radius takes from another
			// figure is no push that makes it catch itself or keeps it from standing again, see nbFigureDef::engageRadius,
			// and no load either but from a ragdoll that lies on it. The fist of one that caught itself, still pressed
			// into the other, would crush it.
			bool fighter = otherFigure != NB_NULL_INDEX && f->def.engageRadius > 0.0f;
			for ( int m = 0; fighter && fighters != NULL && m < contact->manifoldCount; ++m )
			{
				const b3Manifold* manifold = contact->manifolds + m;
				b3Vec3 direction = boneIsA ? b3Neg( manifold->normal ) : manifold->normal;
				b3Vec3 level = b3MulSub( direction, b3Dot( direction, up ), up );
				for ( int p = 0; p < manifold->pointCount; ++p )
				{
					*fighters = b3MulAdd( *fighters, manifold->points[p].totalNormalImpulse / timeStep, level );
				}
			}
			if ( fighter && world->figures.data[otherFigure].state != nb_figureRagdoll )
			{
				continue;
			}

			for ( int m = 0; m < contact->manifoldCount; ++m )
			{
				const b3Manifold* manifold = contact->manifolds + m;

				// The normal points from A to B: the push on the bone
				b3Vec3 direction = boneIsA ? b3Neg( manifold->normal ) : manifold->normal;
				if ( arm && fighter )
				{
					continue;
				}
				if ( arm )
				{
					for ( int p = 0; p < manifold->pointCount; ++p )
					{
						bonePush = b3MulAdd( bonePush, manifold->points[p].totalNormalImpulse, direction );
					}
					continue;
				}

				float along = b3Dot( direction, up );
				if ( along > -0.3f )
				{
					if ( push != NULL && fighter == false && along > -0.7f && along < 0.7f )
					{
						b3Vec3 level = b3MulSub( direction, along, up );
						for ( int p = 0; p < manifold->pointCount; ++p )
						{
							bonePush = b3MulAdd( bonePush, manifold->points[p].totalNormalImpulse, level );
						}
					}
					continue;
				}
				for ( int p = 0; p < manifold->pointCount; ++p )
				{
					force += manifold->points[p].totalNormalImpulse / timeStep;
				}
			}
		}
		float boneForce = b3Length( bonePush ) / timeStep;
		strongest = b3MaxFloat( strongest, boneForce );
		if ( arm && arms != NULL )
		{
			int k = i <= nb_humanoidHandLeft ? 0 : 1;
			arms[k] = b3MaxFloat( arms[k], boneForce );
		}
	}
	if ( push != NULL )
	{
		*push = strongest;
	}
	return force;
}

//----------------------------------------------------------------------------------------------------------------------
// Following the clip near another figure

// Fighters near each other follow their clips with dynamic bones, as the fighters of Blade & Sorcery near the player, see
// the roadmap, F64: a kick meets a block or a body, and what hits a fighter pushes its bones. Far from others a figure
// stays kinematic, which costs Box3D nothing to solve. Every bone has a drive, a motor joint from the static anchor of
// the world: its velocity motors take the bone where the clip puts it, with at most the force the clip needs to move it
// and followStrength times its weight more, so what blocks a bone holds it and what pushes harder moves it. The joints
// hold the pose of the clip at full strength, so a blocking arm stays stiff and gives way with the body behind it.

// The rotation vector in world space that turns one rotation into another, the short way round
static b3Vec3 nbTurnBetween( b3Quat from, b3Quat to )
{
	b3Quat q = b3MulQuat( to, b3Conjugate( from ) );
	if ( q.s < 0.0f )
	{
		q = b3NegateQuat( q );
	}
	float sine = b3Length( q.v );
	if ( sine < 1.0e-6f )
	{
		return b3MulSV( 2.0f, q.v );
	}
	return b3MulSV( 2.0f * b3Atan2( sine, q.s ) / sine, q.v );
}

// Which standing figures that follow their clips near others have another figure near at the start of an update: its
// pelvis within their engage radius of theirs. The pelvises are read once.
static void nbFindEngaged( nbWorld* world )
{
	bool any = false;
	for ( int i = 0; i < world->figures.count && any == false; ++i )
	{
		const nbFigure* f = world->figures.data + i;
		any = f->isFree == false && f->def.engageRadius > 0.0f && f->state == nb_figureAnimated;
	}
	if ( any == false )
	{
		return;
	}

	int count = world->figures.count;
	nbArray_Reserve( world->figurePelvises, count );
	world->figurePelvises.count = count;
	b3Pos* pelvises = world->figurePelvises.data;
	for ( int i = 0; i < count; ++i )
	{
		const nbFigure* f = world->figures.data + i;
		pelvises[i] = f->isFree ? b3ToPos( b3Vec3_zero ) : b3Body_GetPosition( f->bones[0].bodyId );
	}

	for ( int i = 0; i < count; ++i )
	{
		nbFigure* f = world->figures.data + i;
		if ( f->isFree || f->def.engageRadius <= 0.0f || f->state != nb_figureAnimated )
		{
			continue;
		}

		float reach = f->def.engageRadius * f->def.engageRadius;
		f->engaged = false;
		for ( int j = 0; j < count && f->engaged == false; ++j )
		{
			f->engaged = j != i && world->figures.data[j].isFree == false &&
						 b3LengthSquared( b3SubPos( pelvises[j], pelvises[i] ) ) <= reach;
		}
	}
}

// The mass of all bones of a figure
static float nbFigureMass( const nbFigure* f )
{
	float mass = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		mass += f->bones[i].mass;
	}
	return mass;
}

// Does the drive of a bone turn it too? Every drive takes its bone where the clip puts it, the root and the legs of the
// humanoid also turn as the clip turns them: the legs carry the figure, and a leg whose bones were only held where they
// are bent at the knee when the program slid the figure over the ground and the ground held the foot. Spine, neck, head
// and arms turn by their joints and by where their neighbors are: drives on their turn as well held them stiffer than
// their joints, and the two shook each other, the hands by up to 14 mm in the idle clip instead of 2 mm.
static bool nbFollowTurns( const nbFigure* f, int bone )
{
	return bone == 0 || ( f->boneCount == nb_humanoidBoneCount && bone >= nb_humanoidThighLeft );
}

// A standing figure begins to follow its clip with dynamic bones, from where its bones are and with the velocities they
// have: where the clip put them for the end of the last step, or where a figure that caught itself stood again.
static void nbBeginFollowing( nbWorld* world, nbFigure* f )
{
	nbMakeDynamic( f );
	b3BodyId anchor = nbFigureAnchor( world );
	float strength = b3MaxFloat( 0.0f, f->def.motors.followStrength );
	float lever = NB_FOLLOW_LEVER * nbFigureScale( f );
	for ( int i = 0; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		float force = strength * bone->mass * NB_FOLLOW_GRAVITY;
		b3MotorJointDef def = b3DefaultMotorJointDef();
		def.base.bodyIdA = anchor;
		def.base.bodyIdB = bone->bodyId;
		def.base.localFrameB.p = b3Body_GetLocalCenter( bone->bodyId );
		def.base.collideConnected = false;
		def.maxVelocityForce = force;
		def.maxVelocityTorque = nbFollowTurns( f, i ) ? force * lever : 0.0f;
		f->followJoints[i] = b3CreateMotorJoint( world->physicsWorld, &def );
		f->followTargets[i] = b3Body_GetTransform( bone->bodyId );
		f->followVelocities[i] = b3Body_GetLinearVelocity( bone->bodyId );
		b3Body_EnableSleep( bone->bodyId, false );
	}

	for ( int i = 1; i < f->boneCount; ++i )
	{
		f->followRelative[i] = nbJointRotation( f->bones + i, f->pose[i] );
		nbLimitJointRotation( f->bones + i, f->followRelative + i );
	}

	f->following = true;
	f->apartTime = 0.0f;
	f->stuckTime = 0.0f;
	f->overpowered = false;
	f->springs = true;
	nbSetStrength( f, 1.0f );
	f->damping = -1.0f;
	nbSetDamping( f, 0.0f );
	nbActivateAtOnce( f );
	nbReadVelocities( f );
}

// The joints of a figure that follows its clip go for its pose and turn with it: every spring gets how fast the clip turns
// its joint, and damps the turn against that instead of against rest, see b3SphericalJoint_SetTargetVelocity. Without it
// the fist of a punch that swings out in 0.12 s lagged twice as far behind the clip.
static void nbFollowJoints( nbFigure* f, float timeStep )
{
	float inverseStep = 1.0f / timeStep;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		if ( bone->joint == nb_fixedJoint )
		{
			continue;
		}

		b3Quat relative = nbJointRotation( bone, f->pose[i] );
		nbLimitJointRotation( bone, &relative );
		b3Quat before = f->followRelative[i];
		f->followRelative[i] = relative;
		if ( bone->joint == nb_hingeJoint )
		{
			float angle = b3GetTwistAngle( relative );
			b3RevoluteJoint_SetTargetAngle( bone->jointId, angle );
			b3RevoluteJoint_SetTargetSpeed( bone->jointId, inverseStep * b3UnwindAngle( angle - b3GetTwistAngle( before ) ) );
		}
		else
		{
			b3SphericalJoint_SetTargetRotation( bone->jointId, relative );
			b3SphericalJoint_SetTargetVelocity( bone->jointId, b3MulSV( inverseStep, nbTurnBetween( before, relative ) ) );
		}
	}
}

// The drives take every bone where the clip puts it at the end of the next step: at the velocity of the clip, and back to
// the clip from where it is within NB_FOLLOW_TIME. A drive pulls as hard as the clip moves its bone, its mass times the
// acceleration the clip gives it, and followStrength times its weight more against what pushes it: a clip at rest gives
// way to a push, a fast one has the force it takes, and that force is behind the fist that lands. The joints go for the
// pose of the clip.
static void nbFollowClip( nbFigure* f, float timeStep )
{
	float inverseStep = 1.0f / timeStep;
	float inverseTime = 1.0f / b3MaxFloat( NB_FOLLOW_TIME, timeStep );
	float strength = b3MaxFloat( 0.0f, f->def.motors.followStrength ) * NB_FOLLOW_GRAVITY;
	float lever = NB_FOLLOW_LEVER * nbFigureScale( f );
	for ( int i = 0; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		b3Vec3 center = b3Body_GetLocalCenter( bone->bodyId );
		b3WorldTransform transform = b3Body_GetTransform( bone->bodyId );
		b3WorldTransform from = f->followTargets[i];
		b3WorldTransform to = f->targets[i];
		b3Pos fromCenter = b3TransformWorldPoint( from, center );
		b3Vec3 clipVelocity = b3MulSV( inverseStep, b3SubPos( b3TransformWorldPoint( to, center ), fromCenter ) );
		float acceleration =
			b3MinFloat( inverseStep * b3Length( b3Sub( clipVelocity, f->followVelocities[i] ) ), NB_FOLLOW_MAX_ACCELERATION );
		f->followVelocities[i] = clipVelocity;
		b3Vec3 velocity =
			b3MulAdd( clipVelocity, inverseTime, b3SubPos( fromCenter, b3TransformWorldPoint( transform, center ) ) );

		b3JointId jointId = f->followJoints[i];
		float force = bone->mass * ( strength + acceleration );
		b3MotorJoint_SetLinearVelocity( jointId, velocity );
		b3MotorJoint_SetMaxVelocityForce( jointId, force );
		if ( nbFollowTurns( f, i ) )
		{
			b3Vec3 omega = b3MulSV( inverseStep, nbTurnBetween( from.q, to.q ) );
			omega = b3MulAdd( omega, inverseTime, nbTurnBetween( transform.q, from.q ) );
			b3MotorJoint_SetAngularVelocity( jointId, omega );
			b3MotorJoint_SetMaxVelocityTorque( jointId, force * lever );
		}
		f->followTargets[i] = to;
	}
	nbFollowJoints( f, timeStep );
}

// Near another figure a standing figure follows its clip with dynamic bones. Pushed or turned so far that its pelvis is off
// the clip by more than the drives bring back, it lets go and catches itself, and false tells the update so. A while after
// the last figure left and back on its clip, its bones are kinematic again and the clip blends in from where they stand.
static bool nbUpdateFollowing( nbWorld* world, nbFigure* f, float timeStep )
{
	if ( f->following == false )
	{
		if ( f->engaged )
		{
			nbBeginFollowing( world, f );
		}
		return true;
	}

	f->apartTime = f->engaged ? 0.0f : f->apartTime + timeStep;
	b3WorldTransform pelvis = b3Body_GetTransform( f->bones[0].bodyId );
	b3WorldTransform target = f->followTargets[0];
	float distance = b3Length( b3SubPos( pelvis.p, target.p ) );
	float angle = b3Length( nbTurnBetween( target.q, pelvis.q ) );
	float scale = nbFigureScale( f );

	// The bone farthest from the clip
	float farthest = 0.0f;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		b3BodyId bodyId = f->bones[i].bodyId;
		b3Vec3 center = b3Body_GetLocalCenter( bodyId );
		b3Vec3 offset = b3SubPos( b3TransformWorldPoint( b3Body_GetTransform( bodyId ), center ),
								  b3TransformWorldPoint( f->followTargets[i], center ) );
		farthest = b3MaxFloat( farthest, b3Length( offset ) );
	}
	f->stuckTime = farthest > NB_FOLLOW_STUCK * scale ? f->stuckTime + timeStep : 0.0f;

	if ( distance > NB_FOLLOW_LET_GO * scale || angle > NB_FOLLOW_LET_GO_TURN || f->stuckTime >= NB_FOLLOW_STUCK_TIME )
	{
		f->letGoCount += 1;
		nbBeginBalancing( world, f );
		return false;
	}

	if ( f->apartTime >= NB_FOLLOW_HOLD && distance < NB_FOLLOW_SETTLED * scale && angle < NB_FOLLOW_SETTLED_TURN )
	{
		b3WorldTransform root;
		nbReadPose( f, f->pose, &root );
		f->root = b3InvMulWorldTransforms( f->frame, root );
		nbEndFollowing( world, f, true );
		nbStartFade( f, NB_STAND_FADE );
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Update

static void nbUpdateGettingUpPose( nbFigure* f, float advance, float timeStep );

// The bones with a spring of their own swing after a hit, see nbBoneDef::flinch and nbFlinch, parents first. A bone lags
// behind what carries it: what its parent gained in turning over a step it turns back against it, and as its joint
// moves it turns so that its center of mass stays behind, see nbLagCarried. Its spring pulls it after, stepped by its
// exact solution. A bone of the spine turns by its share of the trunk, any other with its parent. When the trunk and
// every swing have come to rest, the swings stop.
static void nbSwingBones( nbFigure* f, float timeStep )
{
	bool moving = b3LengthSquared( f->flinch ) > 1.0e-8f || b3LengthSquared( f->flinchRate ) > 1.0e-6f;
	f->bones[0].turnRate = f->boneCount == nb_humanoidBoneCount ? b3Vec3_zero : f->flinchRate;
	f->bones[0].pivotVelocity = b3Vec3_zero;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		nbBone* bone = f->bones + i;
		const nbBone* parent = f->bones + bone->parent;
		b3Vec3 lever = b3InvRotateVector( f->frame.q, b3SubPos( f->targets[i].p, f->targets[bone->parent].p ) );
		bone->pivotVelocity = b3Add( parent->pivotVelocity, b3Cross( parent->turnRate, lever ) );
		if ( bone->flinchOmega <= 0.0f )
		{
			bone->turnRate =
				nbIsSpine( f, i ) ? b3Add( parent->turnRate, nbSpinePart( f, i, f->flinchRate ) ) : parent->turnRate;
			continue;
		}

		b3Vec3 lag = nbLagCarried( f, i, b3Sub( bone->pivotVelocity, bone->carriedVelocity ) );
		bone->swingRate = b3Add( bone->swingRate, b3Sub( lag, b3Sub( parent->turnRate, bone->carried ) ) );
		bone->carried = parent->turnRate;
		bone->carriedVelocity = bone->pivotVelocity;
		nbStepSpring( &bone->swing, &bone->swingRate, bone->flinchOmega, bone->flinchDamping, timeStep );
		float angle = b3Length( bone->swing );
		if ( angle > f->def.flinchLimit )
		{
			bone->swing = b3MulSV( f->def.flinchLimit / angle, bone->swing );
		}
		bone->turnRate = b3Add( parent->turnRate, bone->swingRate );
		moving = moving || b3LengthSquared( bone->swing ) > 1.0e-8f || b3LengthSquared( bone->swingRate ) > 1.0e-6f;
	}

	if ( moving == false )
	{
		nbStopSwings( f );
	}
}

// Turns the bones of a standing figure by the flinch of the trunk and the swings of the bones with a spring of their own,
// each about its joint in the frame of the figure, parents first: the trunk of the humanoid shared by its spine, the root
// for other rigs. A bone that swings stays within the limits of its joint.
static void nbTurnBones( nbFigure* f )
{
	bool trunk = b3LengthSquared( f->flinch ) > 1.0e-8f;
	if ( trunk && f->boneCount != nb_humanoidBoneCount )
	{
		f->root.q = b3NormalizeQuat( b3MulQuat( nbQuatFromVector( f->flinch ), f->root.q ) );
	}

	f->bones[0].turned = f->root.q;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		nbBone* bone = f->bones + i;
		b3Quat frameQ = b3MulQuat( f->bones[bone->parent].turned, bone->rest.q );
		bool spine = trunk && nbIsSpine( f, i );
		bool swings = bone->flinchOmega > 0.0f && b3LengthSquared( bone->swing ) > 1.0e-10f;
		if ( spine || swings )
		{
			b3Quat rotation = nbQuatFromVector( swings ? bone->swing : nbSpinePart( f, i, f->flinch ) );
			b3Quat local = b3NormalizeQuat( b3InvMulQuat( frameQ, b3MulQuat( rotation, frameQ ) ) );
			f->pose[i] = b3NormalizeQuat( b3MulQuat( local, f->pose[i] ) );
			if ( swings )
			{
				f->pose[i] = nbLimitBoneRotation( bone, f->pose[i] );
			}
		}
		bone->turned = b3MulQuat( frameQ, f->pose[i] );
	}
}

// A steered humanoid with clips of its gait, see nbGaitClipDef: its trunk, arms and head go as the clips have them at
// the phase its gait has after this update, see nbSteerFeet, blended from the slower to the faster clip as far as it
// runs, see NB_RUN_TURN_LAG, and in from the idle clip as it speeds up, see NB_GAIT_EASE. Its legs keep the idle clip,
// its gait places them. Where all its clips move the root, how far they move it from where the idle clip has it, which
// its gait applies after it planned the feet, see nbSteerFeet and nbPlaceFeet.
static void nbGaitClipPose( const nbWorld* world, nbFigure* f, float timeStep )
{
	float run = 0.0f, stance = 0.0f, land = 0.0f;
	float u = f->gaitSpeed > 0.0f ? nbGaitSpeed( world, f, &run, &stance, &land ) : 0.0f;
	nbStepRunTurn( f, run, timeStep );
	float ease = b3MinFloat( 1.0f, u / NB_GAIT_EASE );
	float phase = f->gaitPhase;
	if ( f->gaitOn )
	{
		phase += timeStep * f->gaitPhaseRate;
		phase -= floorf( phase );
	}

	b3Quat pose[nb_humanoidBoneCount];
	b3Transform root;
	nbSampleGaitClip( f, f->gaitClips, phase, pose, &root );
	float fast = f->gaitClipCount > 1 ? f->runTurn : 0.0f;
	if ( fast > 0.0f )
	{
		b3Quat fastPose[nb_humanoidBoneCount];
		b3Transform fastRoot;
		nbSampleGaitClip( f, f->gaitClips + 1, phase, fastPose, &fastRoot );
		for ( int i = nb_humanoidBelly; i < nb_humanoidThighLeft; ++i )
		{
			pose[i] = b3NLerp( pose[i], fastPose[i], fast );
		}
	}
	for ( int i = nb_humanoidBelly; i < nb_humanoidThighLeft; ++i )
	{
		f->pose[i] = b3NLerp( f->pose[i], pose[i], ease );
	}

	// The root runs as low as the faster clip has it no longer than the gait runs, as the crouch of running goes with the
	// speed, see nbSteerFeet
	f->gaitClipEase = ease;
	f->gaitClipFast = b3MinFloat( fast, run );
	f->gaitRoot = b3Transform_identity;
	if ( f->gaitClipRoot )
	{
		float level = f->gaitClips[0].level;
		if ( f->gaitClipFast > 0.0f )
		{
			level += f->gaitClipFast * ( f->gaitClips[1].level - level );
		}
		// Against the stance of the gait, which lies below where the idle clip stands, see NB_STAND_RISE
		b3Vec3 stance = f->root.p;
		stance.y = b3MinFloat( stance.y, f->def.standHeight );
		f->gaitClipLevel = ease * ( level - stance.y );
		root = nbGaitClipRoot( f, phase );
		f->gaitRoot.p = b3MulSV( ease, b3Sub( root.p, stance ) );
		f->gaitRoot.q = b3NLerp( b3Quat_identity, b3MulQuat( root.q, b3Conjugate( f->root.q ) ), ease );
	}
}

// The pose of a standing figure from its clip, the flinch of its trunk and what it blends from
static void nbAnimate( nbWorld* world, int index, nbFigure* f, float timeStep )
{
	NB_UNUSED( index );

	// The clip
	const nbClip* clip = f->playing ? &f->playClip : f->clips[nb_clipIdle];
	f->clipTime += f->clipSpeed * timeStep;
	if ( clip->loop == false )
	{
		f->clipTime = b3MinFloat( f->clipTime, nbClipDuration( clip ) );
	}
	nbSampleFigureClip( f, clip, f->clipTime, f->pose, &f->root );

	// A steered humanoid with clips of its gait moves as they have it
	if ( f->steered && nbGaitClipped( f ) )
	{
		nbGaitClipPose( world, f, timeStep );
	}

	// The fade from the pose before
	if ( f->fadeTime < f->fadeDuration )
	{
		f->fadeTime += timeStep;
		float alpha = b3ClampFloat( f->fadeTime / f->fadeDuration, 0.0f, 1.0f );
		alpha = alpha * alpha * ( 3.0f - 2.0f * alpha );
		for ( int i = 0; i < f->boneCount; ++i )
		{
			f->pose[i] = b3NLerp( f->fadePose[i], f->pose[i], alpha );
		}
		f->root = nbBlendRoots( f->fadeRoot, f->root, alpha );
		if ( f->fadeFromClips )
		{
			f->gaitBlend = alpha;
			f->fadeFromClips = alpha < 1.0f;
		}
	}

	// The flinch: a critically damped spring that straightens the trunk, stepped by its exact solution, so the swing of a
	// hit is what nbFlinch gives it at any step rate, the turn about the axis up the spine twistGain times as fast, see
	// nbComputeTwist. The decay over a step from a series of the same bits everywhere.
	float omega = 2.0f * B3_PI * b3MaxFloat( f->def.flinchHertz, 0.1f );
	b3Vec3 twist = { 0.0f, f->flinch.y, 0.0f };
	b3Vec3 twistRate = { 0.0f, f->flinchRate.y, 0.0f };
	f->flinch.y = 0.0f;
	f->flinchRate.y = 0.0f;
	nbStepSpring( &f->flinch, &f->flinchRate, omega, 1.0f, timeStep );
	nbStepSpring( &twist, &twistRate, f->twistGain * omega, 1.0f, timeStep );
	f->flinch.y = twist.y;
	f->flinchRate.y = twistRate.y;
	f->flinch = nbClampFlinch( f, f->flinch );

	// The bones with a spring of their own swing after a hit and turn with the trunk
	if ( f->swinging )
	{
		nbSwingBones( f, timeStep );
		nbTurnBones( f );
		return;
	}

	// The flinch turns the trunk about the root: shared by the bones of the spine of the humanoid, see nbSpinePart, the root
	// alone for other rigs. The rotation is in the frame of the figure, the spine bones turn by their share in their own
	// frames.
	if ( b3LengthSquared( f->flinch ) > 1.0e-8f )
	{
		if ( f->boneCount == nb_humanoidBoneCount )
		{
			b3Quat frameQ = f->root.q;
			for ( int i = nb_humanoidBelly; i < nb_humanoidBelly + NB_SPINE_COUNT; ++i )
			{
				const nbBone* bone = f->bones + i;
				b3Quat share = nbQuatFromVector( nbSpinePart( f, i, f->flinch ) );
				frameQ = b3MulQuat( frameQ, bone->rest.q );
				b3Quat local = b3NormalizeQuat( b3InvMulQuat( frameQ, b3MulQuat( share, frameQ ) ) );
				f->pose[i] = b3NormalizeQuat( b3MulQuat( local, f->pose[i] ) );
				frameQ = b3MulQuat( frameQ, f->pose[i] );
			}
		}
		else
		{
			f->root.q = b3NormalizeQuat( b3MulQuat( nbQuatFromVector( f->flinch ), f->root.q ) );
		}
	}
}

// A standing figure: its ground, what presses on it, its pose and its bones
static void nbUpdateAnimated( nbWorld* world, int index, nbFigure* f, float timeStep, b3Vec3 up )
{
	// Debris that hit it in the step
	if ( f->debrisImpulse > 0.0f )
	{
		nbTakeHit( world, f, f->debrisBone, b3MulSV( f->debrisImpulse, f->debrisDirection ), f->debrisPoint, true );
		if ( f->state == nb_figureRagdoll || f->state == nb_figureBalancing )
		{
			return;
		}
	}

	// What presses on it from above, in turn with the other figures. What its bones push aside presses back on them, but
	// does not weigh on it. A sideways push at two checks in a row, a few steps, makes a humanoid catch itself: a blow
	// lasts a step or two and is a hit.
	if ( ( world->figureUpdates + index ) % NB_CRUSH_PERIOD == 0 )
	{
		float push = 0.0f;
		float arms[2] = { 0.0f, 0.0f };
		b3Vec3 fighters = b3Vec3_zero;
		if ( nbPressingForce( world, index, f, timeStep, up, &push, arms, f->following ? &fighters : NULL ) >= f->def.crushForce )
		{
			nbFall( world, f, false );
			return;
		}

		// Other figures that push a fighter over the ground harder than it could push back from its feet, with more than
		// half its weight at two checks in a row, overpower its drives: a fist with its elbow stretched, pressed into the
		// body of the other after it landed, would else push both with all their drives. It lets go and catches itself.
		if ( f->following )
		{
			bool overpowered = b3Length( fighters ) >=
							   NB_FOLLOW_OVERPOWER * nbFigureMass( f ) * b3Length( b3World_GetGravity( world->physicsWorld ) );
			if ( overpowered && f->overpowered )
			{
				f->letGoCount += 1;
				nbBeginBalancing( world, f );
				return;
			}
			f->overpowered = overpowered;
		}

		f->armPush[0] = arms[0];
		f->armPush[1] = arms[1];
		bool pushed = f->def.pushForce > 0.0f && push >= f->def.pushForce && f->canBalance;
		if ( pushed && f->pushed )
		{
			f->pushCount += 1;
			nbBeginBalancing( world, f );
			return;
		}
		f->pushed = pushed;
	}

	// Near another figure it follows its clip with dynamic bones, and what pushes them moves them
	if ( f->def.engageRadius > 0.0f && nbUpdateFollowing( world, f, timeStep ) == false )
	{
		return;
	}

	if ( f->steered )
	{
		// A steered figure moves as its capsule, on its own feet, see figure_steer.c
		if ( nbMoveSteered( world, index, f, timeStep, up ) == false )
		{
			nbFall( world, f, false );
			return;
		}
	}
	else
	{
		// A push it catches carries it over the ground
		nbSway( f, timeStep );

		// The ground: a figure follows it up and down a little, and falls where it is gone or gives way
		nbFigureQuery ground = nbFindGround( world, index, f->frame.p, up, NB_GROUND_ABOVE, NB_GROUND_BELOW );
		bool falls = ground.hit == false;
		if ( ground.hit )
		{
			b3BodyId body = b3Shape_GetBody( ground.shapeId );
			if ( b3Body_GetType( body ) != b3_staticBody )
			{
				b3Vec3 velocity = b3Body_GetLinearVelocity( body );
				falls = b3Length( velocity ) > NB_GROUND_SPEED;
			}
		}
		if ( falls )
		{
			nbFall( world, f, false );
			return;
		}

		float height = b3Dot( b3SubPos( ground.point, f->frame.p ), up );
		float step = NB_GROUND_FOLLOW * timeStep;
		f->frame.p = b3OffsetPos( f->frame.p, b3MulSV( b3ClampFloat( height, -step, step ), up ) );
	}

	nbAnimate( world, index, f, timeStep );
	if ( f->steered && f->boneCount == nb_humanoidBoneCount )
	{
		nbComeDownForGait( f, timeStep );
	}
	if ( f->steered && f->swinging == false )
	{
		nbGaitPose( world, f, up, timeStep );
	}

	// The feet of the humanoid on the ground, held where they stand while it steps, and after its last step back where the
	// clip puts them. On flat ground it looks at them only when the ground under them is due.
	bool due = ( world->figureUpdates + index ) % NB_FOOT_PERIOD == 0;
	if ( f->boneCount == nb_humanoidBoneCount && ( due || f->feetFlat == false || f->footWeight > 0.0f || f->steered ) )
	{
		float scale = nbFigureScale( f );
		b3Transform transforms[nb_humanoidBoneCount];
		nbFrameLegs( f, f->root, f->pose, transforms );
		nbClipFoot clips[2] = { nbGetClipFoot( f, transforms, nb_humanoidFootLeft, scale ),
								nbGetClipFoot( f, transforms, nb_humanoidFootRight, scale ) };
		nbFollowGround( world, index, f, clips, due, timeStep, scale );
		if ( f->steered )
		{
			nbSteerFeet( world, index, f, clips, up, timeStep );
		}
		else if ( f->footWeight > 0.0f && nbStepFeet( world, index, f, transforms, clips, up, timeStep ) == false )
		{
			// A step without ground: the figure catches itself with dynamic bones, if it can
			nbBeginBalancing( world, f );
			return;
		}
		nbPlaceFeet( f, transforms, clips, up, timeStep );
		if ( f->balancing == false && f->gaitOn == false )
		{
			f->footWeight = b3MaxFloat( 0.0f, f->footWeight - timeStep / NB_FOOT_FADE );
		}

		// Woken up standing, the legs go from how they lay to where the feet need them over the fade, as the rest of the
		// body: a knee that the slack ragdoll bent in would jump at once
		if ( f->fadeLegs )
		{
			float alpha = b3ClampFloat( f->fadeTime / f->fadeDuration, 0.0f, 1.0f );
			alpha = alpha * alpha * ( 3.0f - 2.0f * alpha );
			for ( int i = nb_humanoidThighLeft; i <= nb_humanoidFootRight; ++i )
			{
				f->pose[i] = b3NLerp( f->fadePose[i], f->pose[i], alpha );
			}
			f->fadeLegs = alpha < 1.0f;
		}
	}

	nbComputePose( f, b3MulWorldTransforms( f->frame, f->root ), f->pose, f->targets );
	if ( f->following )
	{
		nbFollowClip( f, timeStep );
		f->speed = b3Length( nbAverageVelocity( f ) );
	}
	else
	{
		nbDriveBodies( f, timeStep );
	}
}

// How far a flying ragdoll protects itself and how much of that it braces: a sphere cast along its velocity finds what it
// flies at, and the nearer that is the more it protects itself, like the danger of Overgrowth's SetActiveRagdollFallPose.
// What is ahead of its front it braces against with its arms, from the rest it curls up. A figure that curled up falling
// onto its front would roll over its head.
static void nbSenseDanger( nbWorld* world, int index, nbFigure* f, b3Vec3 velocity, float timeStep )
{
	float protect = 0.0f;
	float brace = 0.0f;
	if ( f->speed > NB_DANGER_SPEED )
	{
		b3Pos origin = b3Body_GetWorldCenter( f->bones[0].bodyId );
		b3Vec3 point = b3Vec3_zero;
		b3ShapeProxy proxy = { &point, 1, NB_DANGER_RADIUS };
		nbFigureQuery query = { world, index, false, false, 1.0f, b3Vec3_zero, origin, b3_nullShapeId, false, false };
		b3World_CastShape( world->physicsWorld, origin, &proxy, b3MulSV( NB_DANGER_TIME, velocity ),
						   nbFigureQueryFilter( world, index ), nbClosestHitFcn, &query );
		if ( query.hit )
		{
			// Into its surface, or along the velocity where the cast starts in it
			b3Vec3 hazard = b3LengthSquared( query.normal ) > 0.25f ? b3Neg( query.normal ) : b3MulSV( 1.0f / f->speed, velocity );
			b3Vec3 front = b3RotateVector( b3Body_GetRotation( f->bones[0].bodyId ), b3Vec3_axisZ );
			protect = 1.0f - query.fraction;
			brace = protect * b3MaxFloat( 0.0f, b3Dot( front, hazard ) );
		}
	}

	float rate = NB_PROTECT_RATE * timeStep;
	f->protect += b3ClampFloat( protect - f->protect, -rate, rate );
	f->brace += b3ClampFloat( brace - f->brace, -rate, rate );
}

// Mix a layer into a pose by a weight
static void nbMixPose( b3Quat* pose, const b3Quat* layer, float weight, int count )
{
	if ( weight <= 0.0f )
	{
		return;
	}
	for ( int i = 1; i < count; ++i )
	{
		pose[i] = b3NLerp( pose[i], layer[i], b3MinFloat( weight, 1.0f ) );
	}
}

// Is there room to get up over a point of the ground: to rise over it and to kneel ahead of it along a direction?
static bool nbHasRoom( const nbWorld* world, int index, const nbFigure* f, b3Pos ground, b3Vec3 ahead, b3Vec3 up )
{
	float height = 1.75f * f->def.standHeight;
	b3Vec3 low = b3MulSV( NB_ROOM_FLOOR + NB_ROOM_RADIUS, up );
	b3Vec3 high = b3MulSV( height - NB_ROOM_RADIUS, up );
	if ( nbCapsuleBlocked( world, index, ground, low, high, NB_ROOM_RADIUS, false ) )
	{
		return false;
	}
	b3Vec3 kneel = b3MulSV( NB_ROOM_AHEAD, ahead );
	return nbCapsuleBlocked( world, index, ground, b3Add( low, kneel ), b3Add( b3MulSV( 0.6f * height, up ), kneel ),
							 NB_ROOM_RADIUS, false ) == false;
}

// Room to get up near where a figure lies, after the roadmap of the game, 26.15 and 28.4: a figure with its head at a wall
// or under the edge of a slab moves aside or turns instead of waiting. It looks back along its spine, 30 and 60 cm, to its
// sides, 30 and 60 cm, and turned about its pelvis by a quarter turn either way and by half a turn, in that order. A place
// needs ground under the pelvis there, flat enough and at most 20 cm higher or lower, room to get up there, and a way for
// the body lying on the ground: moved, nothing fixed along the way, turned, nothing fixed where it lies halfway and at the
// end. What moves, the body pushes aside. Returns false where there is none.
static bool nbFindRoom( const nbWorld* world, int index, const nbFigure* f, b3Pos floor, b3Vec3 ahead, b3Vec3 up,
						nbShift* shift, b3Pos* ground )
{
	if ( b3LengthSquared( ahead ) == 0.0f )
	{
		return false;
	}

	float scale = f->def.standHeight / NB_HUMANOID_STAND;
	b3Vec3 side = b3Cross( up, ahead );
	const float candidates[9][3] = {
		{ NB_SHIFT_NEAR, 0.0f, 0.0f }, { NB_SHIFT_FAR, 0.0f, 0.0f },   { 0.0f, NB_SHIFT_NEAR, 0.0f },
		{ 0.0f, -NB_SHIFT_NEAR, 0.0f }, { 0.0f, NB_SHIFT_FAR, 0.0f }, { 0.0f, -NB_SHIFT_FAR, 0.0f },
		{ 0.0f, 0.0f, 0.5f * B3_PI },   { 0.0f, 0.0f, -0.5f * B3_PI }, { 0.0f, 0.0f, B3_PI },
	};

	// The body lying on the ground, along the spine through the pelvis
	b3Pos pelvis = b3Body_GetPosition( f->bones[0].bodyId );
	b3Vec3 lift = b3MulSV( b3Dot( b3SubPos( floor, pelvis ), up ) + NB_LYING_HEIGHT * scale, up );
	float radius = NB_LYING_RADIUS * scale;
	for ( int k = 0; k < 9; ++k )
	{
		b3Vec3 offset = b3MulSV( scale, b3Add( b3MulSV( -candidates[k][0], ahead ), b3MulSV( candidates[k][1], side ) ) );
		float turn = candidates[k][2];
		b3Quat rotation = b3MakeQuatFromAxisAngle( up, turn );
		b3Vec3 turned = b3RotateVector( rotation, ahead );

		// Ground under the pelvis there, about as high as here
		nbFigureQuery there = nbFindGround( world, index, b3OffsetPos( pelvis, offset ), up, 0.1f + NB_SHIFT_RISE * scale,
											0.6f );
		if ( there.hit == false || b3Dot( there.normal, up ) < NB_STEP_FLATNESS ||
			 fabsf( b3Dot( b3SubPos( there.point, floor ), up ) ) > NB_SHIFT_RISE * scale )
		{
			continue;
		}
		if ( nbHasRoom( world, index, f, there.point, turned, up ) == false )
		{
			continue;
		}

		// The way of the body, moved over the ground or turned on it
		b3Vec3 head = b3Add( lift, b3MulSV( NB_LYING_AHEAD * scale, ahead ) );
		b3Vec3 feet = b3Add( lift, b3MulSV( -NB_LYING_BEHIND * scale, ahead ) );
		bool blocked;
		if ( turn == 0.0f )
		{
			b3Vec3 points[2] = { head, feet };
			b3ShapeProxy proxy = { points, 2, radius };
			nbFigureQuery query = { world, index, true, false, 1.0f, b3Vec3_zero, pelvis, b3_nullShapeId, true, true };
			b3World_CastShape( world->physicsWorld, pelvis, &proxy, offset, nbFigureQueryFilter( world, index ), nbClosestHitFcn,
							   &query );
			blocked = query.hit;
		}
		else
		{
			b3Quat half = b3MakeQuatFromAxisAngle( up, 0.5f * turn );
			blocked = nbCapsuleBlocked( world, index, pelvis, b3RotateVector( half, head ), b3RotateVector( half, feet ), radius,
										true ) ||
					  nbCapsuleBlocked( world, index, pelvis, b3RotateVector( rotation, head ), b3RotateVector( rotation, feet ),
										radius, true );
		}
		if ( blocked )
		{
			continue;
		}

		shift->offset = b3MulAdd( offset, b3Dot( b3SubPos( there.point, floor ), up ), up );
		shift->turn = turn;
		*ground = there.point;
		return true;
	}
	return false;
}

// Can a lying figure get up where it lies? Ground under its pelvis, room to rise and kneel, nothing on it. Without room
// the humanoid looks for room near where it lies and moves there, see nbFindRoom.
static nbFigureWait nbCheckGetUp( const nbWorld* world, int index, const nbFigure* f, b3Vec3 up, float timeStep, b3Pos* ground,
								  nbShift* shift )
{
	*shift = (nbShift){ b3Vec3_zero, 0.0f };
	b3Pos pelvis = b3Body_GetWorldCenter( f->bones[0].bodyId );
	nbFigureQuery floor = nbFindGround( world, index, pelvis, up, 0.1f, 0.6f );
	if ( floor.hit == false || b3Dot( floor.normal, up ) < 0.7f )
	{
		return nb_waitGround;
	}
	*ground = floor.point;

	if ( nbPressingForce( world, index, f, timeStep, up, NULL, NULL, NULL ) > 0.0f )
	{
		return nb_waitDebris;
	}

	// Standing up over the pelvis, and kneeling a little ahead along the spine
	b3Vec3 spine = b3RotateVector( b3Body_GetRotation( f->bones[0].bodyId ), b3Vec3_axisY );
	b3Vec3 flat = b3MulSub( spine, b3Dot( spine, up ), up );
	float flatLength = b3Length( flat );
	flat = flatLength > 1.0e-3f ? b3MulSV( 1.0f / flatLength, flat ) : b3Vec3_zero;
	if ( nbHasRoom( world, index, f, floor.point, flat, up ) )
	{
		return nb_waitNone;
	}
	if ( f->boneCount == nb_humanoidBoneCount && nbFindRoom( world, index, f, floor.point, flat, up, shift, ground ) )
	{
		return nb_waitNone;
	}
	return nb_waitRoom;
}

// Does anything but the figure overlap the shape of a bone at a transform in the frame of the figure, a little smaller?
static bool nbBoneBlocked( const nbWorld* world, int index, const nbBone* bone, b3WorldTransform frame, b3Transform transform )
{
	b3WorldTransform at = b3MulWorldTransforms( frame, transform );
	b3Vec3 points[8];
	int count;
	float radius;
	if ( bone->shape == nb_capsuleBone )
	{
		points[0] = b3RotateVector( at.q, bone->start );
		points[1] = b3RotateVector( at.q, bone->end );
		count = 2;
		radius = b3MaxFloat( bone->radius - NB_WAKE_SKIN, 0.0f );
	}
	else
	{
		b3Vec3 h = { b3MaxFloat( bone->halfExtents.x - NB_WAKE_SKIN, 0.0f ), b3MaxFloat( bone->halfExtents.y - NB_WAKE_SKIN, 0.0f ),
					 b3MaxFloat( bone->halfExtents.z - NB_WAKE_SKIN, 0.0f ) };
		for ( int k = 0; k < 8; ++k )
		{
			b3Vec3 corner = { k & 1 ? h.x : -h.x, k & 2 ? h.y : -h.y, k & 4 ? h.z : -h.z };
			points[k] = b3RotateVector( at.q, b3Add( bone->center, corner ) );
		}
		count = 8;
		radius = 0.0f;
	}
	b3ShapeProxy proxy = { points, count, radius };
	nbFigureQuery query = { world, index, false, false, 1.0f, b3Vec3_zero, at.p, b3_nullShapeId, false, false };
	b3World_OverlapShape( world->physicsWorld, at.p, &proxy, nbFigureQueryFilter( world, index ), nbAnyOverlapFcn, &query );
	return query.hit;
}

// Does a ragdoll at rest stand on its feet with room to stand there, see NB_WAKE_HEIGHT? Then the height of the ground
// under its feet and where it faces over the ground, its pelvis or else its chest.
static bool nbCheckWakeStanding( const nbWorld* world, int index, const nbFigure* f, b3Vec3 up, float timeStep,
								 float* groundHeight, b3Vec3* forward )
{
	if ( f->boneCount != nb_humanoidBoneCount )
	{
		return false;
	}

	float heights[2];
	for ( int foot = 0; foot < 2; ++foot )
	{
		if ( nbFootOnGround( world, index, f, foot, up, heights + foot ) == false )
		{
			return false;
		}
	}
	float height = 0.5f * ( heights[0] + heights[1] );

	b3Vec3 pelvis = b3ToVec3( b3Body_GetWorldCenter( f->bones[0].bodyId ) );
	b3Vec3 chest = b3ToVec3( b3Body_GetWorldCenter( f->bones[nb_humanoidChest].bodyId ) );
	b3Vec3 spine = b3Normalize( b3Sub( chest, pelvis ) );
	float upright = b3ComputeCosSin( NB_WAKE_TILT ).cosine;
	if ( b3Dot( pelvis, up ) - height < NB_WAKE_HEIGHT * f->def.standHeight || b3Dot( spine, up ) < upright )
	{
		return false;
	}
	if ( nbPressingForce( world, index, f, timeStep, up, NULL, NULL, NULL ) > 0.0f )
	{
		return false;
	}

	b3Vec3 facing = b3RotateVector( b3Body_GetRotation( f->bones[0].bodyId ), b3Vec3_axisZ );
	facing = b3MulSub( facing, b3Dot( facing, up ), up );
	if ( b3LengthSquared( facing ) < 0.01f )
	{
		facing = b3RotateVector( b3Body_GetRotation( f->bones[nb_humanoidChest].bodyId ), b3Vec3_axisZ );
		facing = b3MulSub( facing, b3Dot( facing, up ), up );
	}
	if ( b3LengthSquared( facing ) < 1.0e-6f )
	{
		return false;
	}
	facing = b3Normalize( facing );

	// Room for the trunk and the head in the first pose of the stand clip, on the ground between the ankles: the legs
	// stand there already, and an arm may brush the wall the figure leans on
	b3Vec3 ankles = b3MulSV( 0.5f, b3Add( b3ToVec3( b3Body_GetPosition( f->bones[nb_humanoidFootLeft].bodyId ) ),
										   b3ToVec3( b3Body_GetPosition( f->bones[nb_humanoidFootRight].bodyId ) ) ) );
	b3Pos ground = b3ToPos( b3MulAdd( ankles, height - b3Dot( ankles, up ), up ) );
	b3WorldTransform level = nbMakeFrame( world, ground, 0.0f );
	b3Vec3 ahead = b3InvRotateVector( level.q, facing );
	b3WorldTransform frame = nbMakeFrame( world, ground, b3Atan2( ahead.x, ahead.z ) );
	b3Quat pose[nb_humanoidBoneCount];
	b3Transform root;
	nbSampleFigureClip( f, f->clips[nb_clipIdle], 0.0f, pose, &root );
	b3Transform transforms[nb_humanoidBoneCount];
	nbFramePose( f, root, pose, transforms );
	for ( int i = nb_humanoidPelvis; i <= nb_humanoidHead; ++i )
	{
		if ( nbBoneBlocked( world, index, f->bones + i, frame, transforms[i] ) )
		{
			return false;
		}
	}

	*groundHeight = height;
	*forward = facing;
	return true;
}

// A figure that catches itself: the pose of its clip goes on, balance sets its legs, and it falls or stands again. What
// presses on it from above knocks it down from a quarter of the crush force, as a figure that gets up: catching itself it
// does not carry debris.
static void nbUpdateBalancing( nbWorld* world, int index, nbFigure* f, float timeStep, b3Vec3 up )
{
	// What still pushes it keeps it dynamic, see nbFigureDef::pushForce, and an arm that something holds goes slack, see
	// nbHoldArms
	if ( ( world->figureUpdates + index ) % NB_CRUSH_PERIOD == 0 )
	{
		float push = 0.0f;
		float arms[2] = { 0.0f, 0.0f };
		if ( nbPressingForce( world, index, f, timeStep, up, &push, arms, NULL ) >= NB_ABORT_PART * f->def.crushForce )
		{
			nbFall( world, f, false );
			return;
		}
		f->pushed = f->def.pushForce > 0.0f && push >= 0.5f * f->def.pushForce;
		f->armPush[0] = arms[0];
		f->armPush[1] = arms[1];
	}

	const nbClip* clip = f->playing ? &f->playClip : f->clips[nb_clipIdle];
	f->clipTime += f->clipSpeed * timeStep;
	if ( clip->loop == false )
	{
		f->clipTime = b3MinFloat( f->clipTime, nbClipDuration( clip ) );
	}
	nbSampleFigureClip( f, clip, f->clipTime, f->pose, &f->root );

	nbBalanceResult result = nbUpdateBalance( world, index, f, timeStep, f->pose );
	if ( result == nb_balanceFell )
	{
		nbFall( world, f, false );
		return;
	}
	if ( result == nb_balanceSettled )
	{
		nbSettle( world, f );
		return;
	}

	nbSetJointTargets( f, f->pose );
	nbUpdateSoftness( f, timeStep );
	nbUpdateMuscles( f, timeStep );
	f->speed = b3Length( nbAverageVelocity( f ) );
}

// A ragdoll: its strength, its curl and brace, the targets of its joints, its rest and whether it wakes up standing or
// gets up
static void nbUpdateRagdoll( nbWorld* world, int index, nbFigure* f, float timeStep, b3Vec3 up )
{
	f->stun = b3MaxFloat( 0.0f, f->stun - timeStep );
	bool awake = b3Body_IsAwake( f->bones[0].bodyId );
	b3Vec3 velocity = awake ? nbAverageVelocity( f ) : b3Vec3_zero;
	f->speed = b3Length( velocity );

	if ( awake )
	{
		// The strength of Overgrowth: as fast as it flies, slack after a hard hit
		float strength = b3ClampFloat( f->speed * f->def.motors.strengthPerSpeed, 0.0f, f->def.motors.fallStrength );
		f->strength = b3MaxFloat( 0.0f, strength - f->stun );

		nbSenseDanger( world, index, f, velocity, timeStep );

		// The pose to fall with, as far as the figure is strong, relaxed toward its rest pose as far as it is not: a slow one
		// mostly lies, and one that flailed on the ground would crawl over it with its own strength. Then curled up and
		// braced as far as it is.
		f->fallTime += timeStep;
		b3Quat* pose = f->pose;
		b3Quat* layer = f->fadePose;
		b3Transform root;
		nbSampleFigureClip( f, f->clips[nb_clipFall], f->fallTime, pose, &root );
		float fallStrength = f->def.motors.fallStrength;
		float flail = fallStrength > 0.0f ? b3ClampFloat( f->strength / fallStrength, 0.0f, 1.0f ) : 0.0f;
		for ( int i = 1; i < f->boneCount; ++i )
		{
			pose[i] = nbPartOf( pose[i], flail );
		}
		float curl = f->protect - f->brace;
		if ( curl > 0.0f )
		{
			nbSampleFigureClip( f, f->clips[nb_clipCurl], f->fallTime, layer, &root );
			nbMixPose( pose, layer, curl, f->boneCount );
		}
		if ( f->brace > 0.0f )
		{
			nbSampleFigureClip( f, f->clips[nb_clipBrace], f->fallTime, layer, &root );
			nbMixPose( pose, layer, f->brace, f->boneCount );
		}
		for ( int i = 1; i < f->boneCount; ++i )
		{
			pose[i] = nbLimitBoneRotation( f->bones + i, pose[i] );
		}
		nbSetJointTargets( f, pose );
		nbSetStrength( f, f->strength );
		nbUpdateMuscles( f, timeStep );

		// Lying still it slows down and falls asleep
		f->restTime = f->speed < f->def.restSpeed ? f->restTime + timeStep : 0.0f;
		nbSetDamping( f, b3MinFloat( 1.0f, NB_REST_DAMPING_RATE * f->restTime ) * NB_REST_DAMPING );
	}
	else
	{
		f->restTime += timeStep;
		f->strength = 0.0f;
		for ( int i = 0; i < f->boneCount; ++i )
		{
			f->bones[i].stepVelocity = b3Vec3_zero;
		}
	}

	// Getting up, checked every eighth update in turn with the other figures
	if ( f->def.canGetUp == false )
	{
		f->wait = nb_waitStaysDown;
		return;
	}
	if ( f->stateTime < f->def.getUpDelay || f->restTime < NB_REST_TO_GET_UP )
	{
		f->wait = nb_waitMoving;
		return;
	}
	if ( ( world->figureUpdates + index ) % 8 != 0 && f->wait != nb_waitMoving )
	{
		return;
	}

	// Standing on its feet it wakes up standing
	float standGround;
	b3Vec3 forward;
	if ( nbCheckWakeStanding( world, index, f, up, timeStep, &standGround, &forward ) )
	{
		nbStandWhereItIs( world, f, standGround, forward, NB_WAKE_FADE, true );
		f->wait = nb_waitNone;
		f->getUpCount += 1;
		return;
	}

	b3Pos ground;
	nbShift shift;
	f->wait = nbCheckGetUp( world, index, f, up, timeStep, &ground, &shift );
	if ( f->wait == nb_waitNone )
	{
		nbStartGetUp( world, index, f, ground, up, shift );
		if ( f->shiftLeft > 0.0f )
		{
			nbComputePose( f, f->shiftFrom, f->lyingPose, f->targets );
			nbDriveBodies( f, timeStep );
		}
		else
		{
			nbUpdateGettingUpPose( f, 0.0f, timeStep );
		}
	}
}

// A rotation a share of the way from one to another, along the side the signs of the two give, as b3NLerp takes it
// after it chose the shorter side
static b3Quat nbBlendQuats( b3Quat from, b3Quat to, float alpha )
{
	b3Quat q;
	q.v = b3Lerp( from.v, to.v, alpha );
	q.s = ( 1.0f - alpha ) * from.s + alpha * to.s;
	return b3NormalizeQuat( q );
}

// The pose of a figure that gets up: its get-up clip, blended in from the pose the ragdoll lay in over the first part of
// the clip, the root as well as the bones in the frames of their parents, as Overgrowth blends from the ragdoll. A bone
// turns from how it lay toward the clip the way that was shorter as it began to get up, see nbStartGetUp, and stays on
// it: one that lay about half a turn from the clip would otherwise flip to the other way whenever the clip moves it past
// half a turn, and jump. The sign of the clip stays on the side of its sample the step before, which waits in the fade
// pose, a figure that gets up does not fade.
static void nbUpdateGettingUpPose( nbFigure* f, float advance, float timeStep )
{
	const nbClip* clip = f->clips[f->getUpClip];
	f->getUpTime += b3MaxFloat( f->def.getUpSpeed, 0.01f ) * advance;
	nbSampleFigureClip( f, clip, f->getUpTime, f->pose, &f->root );

	float blend = b3ClampFloat( f->getUpTime / NB_GET_UP_BLEND, 0.0f, 1.0f );
	blend = blend * blend * ( 3.0f - 2.0f * blend );
	b3WorldTransform clipRoot = b3MulWorldTransforms( f->frame, f->root );
	b3WorldTransform root = { b3LerpPosition( f->lyingRoot.p, clipRoot.p, blend ), b3NLerp( f->lyingRoot.q, clipRoot.q, blend ) };
	for ( int i = 1; i < f->boneCount; ++i )
	{
		b3Quat sample = b3DotQuat( f->pose[i], f->fadePose[i] ) < 0.0f ? b3NegateQuat( f->pose[i] ) : f->pose[i];
		f->fadePose[i] = sample;
		f->pose[i] = nbBlendQuats( f->lyingPose[i], sample, blend );
	}
	nbComputePose( f, root, f->pose, f->targets );
	nbDriveBodies( f, timeStep );
}

// A figure that gets up: debris or a body on it breaks it off, else the clip plays to its end and the figure stands
static void nbUpdateGettingUp( nbWorld* world, int index, nbFigure* f, float timeStep, b3Vec3 up )
{
	if ( f->debrisImpulse > 0.0f )
	{
		nbTakeHit( world, f, f->debrisBone, b3MulSV( f->debrisImpulse, f->debrisDirection ), f->debrisPoint, true );
		if ( f->state == nb_figureRagdoll || f->state == nb_figureBalancing )
		{
			return;
		}
	}

	if ( ( world->figureUpdates + index ) % NB_CRUSH_PERIOD == 0 &&
		 nbPressingForce( world, index, f, timeStep, up, NULL, NULL, NULL ) >= NB_ABORT_PART * f->def.crushForce )
	{
		nbFall( world, f, false );
		return;
	}

	// Moving where it has room: the pose it lies in, the root moved and turned about up over the shift time
	if ( f->shiftLeft > 0.0f )
	{
		f->shiftLeft = b3MaxFloat( 0.0f, f->shiftLeft - timeStep );
		float u = 1.0f - f->shiftLeft / f->shiftTime;
		float smooth = u * u * ( 3.0f - 2.0f * u );
		b3Quat turn = b3MakeQuatFromAxisAngle( up, ( smooth - 1.0f ) * f->shiftTurn );
		b3WorldTransform root = { b3LerpPosition( f->shiftFrom.p, f->lyingRoot.p, smooth ),
								  b3NormalizeQuat( b3MulQuat( turn, f->lyingRoot.q ) ) };
		nbComputePose( f, root, f->lyingPose, f->targets );
		nbDriveBodies( f, timeStep );
		return;
	}

	nbUpdateGettingUpPose( f, timeStep, timeStep );
	if ( f->getUpTime >= nbClipDuration( f->clips[f->getUpClip] ) )
	{
		nbStand( world, f );
	}
}

void nbUpdateFigures( nbWorld* world, float timeStep )
{
	if ( world->figures.count == 0 || timeStep <= 0.0f )
	{
		return;
	}

	world->figureUpdates += 1;
	b3Vec3 up = nbUpOf( world );
	nbCollectDebrisHits( world );
	nbFindEngaged( world );
	for ( int index = 0; index < world->figures.count; ++index )
	{
		nbFigure* f = world->figures.data + index;
		if ( f->isFree )
		{
			continue;
		}

		f->stateTime += timeStep;
		switch ( f->state )
		{
			case nb_figureAnimated:
				nbUpdateAnimated( world, index, f, timeStep, up );
				break;
			case nb_figureRagdoll:
				nbUpdateRagdoll( world, index, f, timeStep, up );
				break;
			case nb_figureBalancing:
				nbUpdateBalancing( world, index, f, timeStep, up );
				break;
			default:
				nbUpdateGettingUp( world, index, f, timeStep, up );
				break;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Create and destroy

// The clips of the library for a humanoid that stands this high, built the first time a figure asks for them
static const nbClipSet* nbGetHumanoidClips( nbWorld* world, float standHeight )
{
	for ( int i = 0; i < world->humanoidClips.count; ++i )
	{
		if ( world->humanoidClips.data[i]->height == standHeight )
		{
			return world->humanoidClips.data[i];
		}
	}

	nbClipSet* set = nbAlloc( sizeof( nbClipSet ) );
	memset( set, 0, sizeof( nbClipSet ) );
	set->height = standHeight;
	nbKeyframe* keyframes = nbAlloc( sizeof( nbKeyframe ) * NB_HUMANOID_CLIP_KEYFRAMES );
	size_t rotationSize = sizeof( b3Quat ) * NB_HUMANOID_CLIP_KEYFRAMES * nb_humanoidBoneCount;
	b3Quat* rotations = nbAlloc( rotationSize );
	float height = standHeight * NB_HUMANOID_HEIGHT / NB_HUMANOID_STAND;
	for ( int k = 0; k < nb_figureClipCount; ++k )
	{
		nbClipDef clip;
		if ( nbBuildHumanoidClip( (nbFigureClip)k, height, keyframes, rotations, NB_HUMANOID_CLIP_KEYFRAMES, &clip ) > 0 )
		{
			nbCopyClip( set->clips + k, &clip, nb_humanoidBoneCount );
		}
	}
	nbFree( rotations, rotationSize );
	nbFree( keyframes, sizeof( nbKeyframe ) * NB_HUMANOID_CLIP_KEYFRAMES );
	nbArray_Push( world->humanoidClips, set );
	return set;
}

nbFigureId nbCreateFigure( nbWorldId worldId, const nbFigureDef* def )
{
	NB_ASSERT( def->internalValue == NB_SECRET_COOKIE );
	nbWorld* world = nbGetWorldFromId( worldId );
	const nbRigDef* rig = def->rig;
	if ( world == NULL || rig == NULL || rig->bones == NULL || rig->boneCount <= 0 )
	{
		return nb_nullFigureId;
	}
	NB_ASSERT( rig->internalValue == NB_SECRET_COOKIE );

	// The root first, every other bone after its parent
	for ( int i = 0; i < rig->boneCount; ++i )
	{
		int parent = rig->bones[i].parent;
		if ( i == 0 ? parent != -1 : ( parent < 0 || parent >= i ) )
		{
			return nb_nullFigureId;
		}
	}

	int index;
	if ( world->freeFigures.count > 0 )
	{
		index = world->freeFigures.data[--world->freeFigures.count];
	}
	else
	{
		nbFigure blank = { 0 };
		nbArray_Push( world->figures, blank );
		index = world->figures.count - 1;
	}

	int boneCount = rig->boneCount;
	nbFigure* f = world->figures.data + index;
	uint16_t generation = f->generation;
	memset( f, 0, sizeof( nbFigure ) );
	f->generation = generation;
	f->boneCount = boneCount;
	f->def = *def;
	f->def.rig = NULL;
	for ( int k = 0; k < nb_figureClipCount; ++k )
	{
		f->def.clips[k] = NULL;
	}
	f->def.gaitClips = NULL;
	f->def.gaitClipCount = 0;
	f->bones = nbAlloc( sizeof( nbBone ) * (size_t)boneCount );
	f->pose = nbAlloc( sizeof( b3Quat ) * (size_t)boneCount );
	f->fadePose = nbAlloc( sizeof( b3Quat ) * (size_t)boneCount );
	f->lyingPose = nbAlloc( sizeof( b3Quat ) * (size_t)boneCount );
	f->targets = nbAlloc( sizeof( b3WorldTransform ) * (size_t)boneCount );
	if ( def->engageRadius > 0.0f )
	{
		f->followJoints = nbAlloc( sizeof( b3JointId ) * (size_t)boneCount );
		f->followTargets = nbAlloc( sizeof( b3WorldTransform ) * (size_t)boneCount );
		f->followVelocities = nbAlloc( sizeof( b3Vec3 ) * (size_t)boneCount );
		f->followRelative = nbAlloc( sizeof( b3Quat ) * (size_t)boneCount );
	}
	f->collisionDamage = rig->enableCollisionDamage;

	// The clips of the program copied, the library's for the humanoid shared, a rig of other bones without a clip at rest
	const nbClipSet* library = NULL;
	for ( int k = 0; k < nb_figureClipCount; ++k )
	{
		f->clips[k] = f->ownClips + k;
		if ( def->clips[k] != NULL )
		{
			nbCopyClip( f->ownClips + k, def->clips[k], boneCount );
			continue;
		}
		if ( boneCount == nb_humanoidBoneCount )
		{
			library = library != NULL ? library : nbGetHumanoidClips( world, def->standHeight );
			f->clips[k] = library->clips + k;
		}
	}
	nbCopyGaitClips( f, def->gaitClips, def->gaitClipCount );
	f->gaitRoot = b3Transform_identity;
	f->gaitBlend = 1.0f;

	for ( int i = 0; i < boneCount; ++i )
	{
		const nbBoneDef* boneDef = rig->bones + i;
		nbBone* bone = f->bones + i;
		bone->bodyId = b3_nullBodyId;
		bone->jointId = b3_nullJointId;
		bone->parent = boneDef->parent;
		bone->joint = i == 0 ? nb_fixedJoint : boneDef->joint;
		bone->rest = boneDef->restTransform;
		bone->rest.q = b3NormalizeQuat( bone->rest.q );
		bone->jointFrame = b3NormalizeQuat( boneDef->jointFrame );
		bone->lowerHalf = (b3CosSin){ 1.0f, 0.0f };
		bone->upperHalf = (b3CosSin){ 1.0f, 0.0f };
		bone->coneHalf = (b3CosSin){ 1.0f, 0.0f };
		bone->swingLimits = false;
		for ( int k = 0; k < 4; ++k )
		{
			bone->swingInverseTans[k] = 0.0f;
		}
		bone->swingInside = 0.0f;
		bone->strength = boneDef->strength;
		bone->brace = 1.0f;
		bone->stiffness = b3MaxFloat( 0.0f, boneDef->stiffness );
		bone->damping = b3MaxFloat( 0.0f, boneDef->damping );
		bone->catchDamping = b3MaxFloat( 0.0f, boneDef->catchDamping );
		bone->hitStiffness = boneDef->joint == nb_fixedJoint || i == 0 ? 1.0f : b3ClampFloat( boneDef->hitStiffness, 0.0f, 1.0f );
		bone->armature = NB_JOINT_ARMATURE ? b3MaxFloat( 0.0f, boneDef->armature ) : 0.0f;
		bone->maxSpeed = b3MaxFloat( 0.0f, boneDef->maxSpeed );
		bone->drive = 0.0f;
		bone->activation = 0.0f;
		bone->springOn = false;
		bone->springHertz = -1.0f;
		bone->springDamping = -1.0f;
		bone->springTorque = -1.0f;
		bone->hingeAngle = 0.0f;
		bone->mass = b3MaxFloat( boneDef->mass, 1.0e-3f );
		bone->shape = boneDef->shape;
		bone->center = boneDef->center;
		bone->halfExtents = boneDef->halfExtents;
		bone->start = boneDef->start;
		bone->end = boneDef->end;
		bone->radius = boneDef->radius;

		// The spring that swings it from a hit, none for the root and the spine of the humanoid, which the flinch of the
		// trunk turns
		bool trunk =
			i == 0 || ( boneCount == nb_humanoidBoneCount && i >= nb_humanoidBelly && i < nb_humanoidBelly + NB_SPINE_COUNT );
		bone->flinch = b3MaxFloat( 0.0f, boneDef->flinch );
		bone->flinchOmega = trunk == false && boneDef->flinchHertz > 0.0f ? 2.0f * B3_PI * boneDef->flinchHertz : 0.0f;
		bone->flinchDamping = b3ClampFloat( boneDef->flinchDamping, 0.05f, 1.0f );
		bone->hangs = false;
		bone->swing = b3Vec3_zero;
		bone->swingRate = b3Vec3_zero;
		bone->turnRate = b3Vec3_zero;
		bone->pivotVelocity = b3Vec3_zero;
		bone->carried = b3Vec3_zero;
		bone->carriedVelocity = b3Vec3_zero;
		bone->turned = b3Quat_identity;
		f->hasSwings = f->hasSwings || bone->flinchOmega > 0.0f;
	}

	// What a bone with a spring carries hangs beside the body when its center of mass at rest lies a tenth of the stand
	// height or more beside the axis up through the root, an arm, see nbLagCarried
	for ( int b = 1; b < boneCount && f->hasSwings; ++b )
	{
		nbBone* bone = f->bones + b;
		b3Vec3 moment = b3Vec3_zero;
		float mass = 0.0f;
		for ( int i = b; i < boneCount; ++i )
		{
			if ( nbCarries( f, b, i ) )
			{
				const nbBone* other = f->bones + i;
				b3Vec3 local = other->shape == nb_capsuleBone ? b3MulSV( 0.5f, b3Add( other->start, other->end ) )
															  : other->center;
				moment = b3MulAdd( moment, other->mass, b3TransformPoint( nbRestTransform( f, i ), local ) );
				mass += other->mass;
			}
		}
		b3Vec3 center = b3MulSV( 1.0f / mass, moment );
		float reach = NB_SWING_BESIDE * def->standHeight;
		bone->hangs = bone->flinchOmega > 0.0f && center.x * center.x + center.z * center.z >= reach * reach;
	}

	// The first pose of the idle clip where the figure stands, the feet where the clip puts them
	nbResetFeet( f );
	f->heading = def->heading;
	f->frame = nbMakeFrame( world, def->position, def->heading );
	f->clipSpeed = 1.0f;
	f->state = nb_figureAnimated;
	nbSampleFigureClip( f, f->clips[nb_clipIdle], 0.0f, f->pose, &f->root );
	nbComputePose( f, b3MulWorldTransforms( f->frame, f->root ), f->pose, f->targets );
	nbInitSteering( f, f->targets );

	// Kinematic bodies in that pose. The bones of the figure are in a collision group of their own, see NB_FIGURE_GROUP:
	// without self collision negative, which Box3D keeps apart, with it positive, which collides as without a group. Its
	// queries leave the group out, see nbFigureQueryFilter. All shapes report hits, so debris that hits a standing figure
	// shows in the events, see nbCollectDebrisHits.
	int groupIndex = rig->selfCollision ? NB_FIGURE_GROUP + index : -( NB_FIGURE_GROUP + index );
	f->groupIndex = groupIndex;
	for ( int i = 0; i < boneCount; ++i )
	{
		const nbBoneDef* boneDef = rig->bones + i;
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_kinematicBody;
		bodyDef.position = f->targets[i].p;
		bodyDef.rotation = f->targets[i].q;
		bodyDef.name = boneDef->name;
		bodyDef.userData = def->userData;
		b3BodyId bodyId = b3CreateBody( world->physicsWorld, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.name = boneDef->name;
		shapeDef.baseMaterial.friction = boneDef->friction > 0.0f ? boneDef->friction : rig->friction;
		shapeDef.enableHitEvents = true;
		shapeDef.filter.groupIndex = groupIndex;
		float volume = b3MaxFloat( nbBoneVolume( boneDef ), 1.0e-6f );
		shapeDef.density = f->bones[i].mass / volume;
		if ( boneDef->shape == nb_boxBone )
		{
			b3BoxHull box = b3MakeOffsetBoxHull( boneDef->halfExtents.x, boneDef->halfExtents.y, boneDef->halfExtents.z,
												 boneDef->center );
			b3CreateHullShape( bodyId, &shapeDef, &box.base );
		}
		else if ( boneDef->shape == nb_roundedBoxBone )
		{
			b3HullData* hull = nbCreateBoneHull( boneDef );
			shapeDef.density = f->bones[i].mass / b3MaxFloat( hull->volume, 1.0e-6f );
			b3CreateHullShape( bodyId, &shapeDef, hull );
			b3DestroyHull( hull );
		}
		else
		{
			b3Capsule capsule = { boneDef->start, boneDef->end, boneDef->radius };
			b3CreateCapsuleShape( bodyId, &shapeDef, &capsule );
		}

		f->bones[i].bodyId = bodyId;
		nbMapBone( world, bodyId, index, i );
	}

	// Joints: frame A in the parent at the pivot, turned like the bone at rest, frame B at the origin of the bone. Both
	// coincide at rest, so the rest pose is the zero of every angle. Between kinematic bones Box3D keeps them aside and
	// solves nothing, until the figure falls.
	for ( int i = 1; i < boneCount; ++i )
	{
		const nbBoneDef* boneDef = rig->bones + i;
		nbBone* bone = f->bones + i;
		b3Transform frameA = { bone->rest.p, b3MulQuat( bone->rest.q, bone->jointFrame ) };
		b3Transform frameB = { b3Vec3_zero, bone->jointFrame };
		b3BodyId parentId = f->bones[bone->parent].bodyId;
		switch ( bone->joint )
		{
			case nb_hingeJoint:
			{
				b3RevoluteJointDef jointDef = b3DefaultRevoluteJointDef();
				jointDef.base.bodyIdA = parentId;
				jointDef.base.bodyIdB = bone->bodyId;
				jointDef.base.localFrameA = frameA;
				jointDef.base.localFrameB = frameB;
				jointDef.base.collideConnected = false;
				jointDef.base.constraintDampingRatio = NB_JOINT_DAMPING_RATIO;
				jointDef.enableLimit = true;
				jointDef.lowerAngle = boneDef->lowerAngle;
				jointDef.upperAngle = boneDef->upperAngle;
#if NB_JOINT_ARMATURE
				jointDef.armature = bone->armature;
#endif
				bone->jointId = b3CreateRevoluteJoint( world->physicsWorld, &jointDef );
				bone->lowerHalf = b3ComputeCosSin( 0.5f * b3RevoluteJoint_GetLowerLimit( bone->jointId ) );
				bone->upperHalf = b3ComputeCosSin( 0.5f * b3RevoluteJoint_GetUpperLimit( bone->jointId ) );
				break;
			}
			case nb_ballJoint:
			{
				// A limit in each of four directions takes the place of the cone
				bool swingLimits = boneDef->lowerSwingX != 0.0f || boneDef->upperSwingX != 0.0f ||
								   boneDef->lowerSwingY != 0.0f || boneDef->upperSwingY != 0.0f;
				float coneAngle = boneDef->coneAngle;
				if ( swingLimits && NB_SWING_LIMITS == 0 )
				{
					coneAngle = b3MaxFloat( b3MaxFloat( -boneDef->lowerSwingX, boneDef->upperSwingX ),
											b3MaxFloat( -boneDef->lowerSwingY, boneDef->upperSwingY ) );
					swingLimits = false;
				}
				b3SphericalJointDef jointDef = b3DefaultSphericalJointDef();
				jointDef.base.bodyIdA = parentId;
				jointDef.base.bodyIdB = bone->bodyId;
				jointDef.base.localFrameA = frameA;
				jointDef.base.localFrameB = frameB;
				jointDef.base.collideConnected = false;
				jointDef.base.constraintDampingRatio = NB_JOINT_DAMPING_RATIO;
				jointDef.enableConeLimit = swingLimits == false;
				jointDef.coneAngle = b3ClampFloat( coneAngle, 0.0f, 0.5f * B3_PI );
#if NB_SWING_LIMITS
				jointDef.enableSwingLimit = swingLimits;
				jointDef.lowerSwingX = boneDef->lowerSwingX;
				jointDef.upperSwingX = boneDef->upperSwingX;
				jointDef.lowerSwingY = boneDef->lowerSwingY;
				jointDef.upperSwingY = boneDef->upperSwingY;
#endif
				jointDef.enableTwistLimit = true;
				jointDef.lowerTwistAngle = boneDef->lowerAngle;
				jointDef.upperTwistAngle = boneDef->upperAngle;
#if NB_JOINT_ARMATURE
				jointDef.armature = bone->armature;
#endif
				bone->jointId = b3CreateSphericalJoint( world->physicsWorld, &jointDef );
				bone->lowerHalf = b3ComputeCosSin( 0.5f * b3SphericalJoint_GetLowerTwistLimit( bone->jointId ) );
				bone->upperHalf = b3ComputeCosSin( 0.5f * b3SphericalJoint_GetUpperTwistLimit( bone->jointId ) );
				bone->coneHalf = b3ComputeCosSin( 0.5f * b3SphericalJoint_GetConeLimit( bone->jointId ) );
#if NB_SWING_LIMITS
				if ( swingLimits )
				{
					float lowerX, upperX, lowerY, upperY;
					b3SphericalJoint_GetSwingLimits( bone->jointId, &lowerX, &upperX, &lowerY, &upperY );
					float tans[4] = { b3TanQuarter( upperX ), b3TanQuarter( -lowerX ), b3TanQuarter( upperY ),
									  b3TanQuarter( -lowerY ) };
					bone->swingLimits = true;
					float least = tans[0];
					for ( int k = 0; k < 4; ++k )
					{
						bone->swingInverseTans[k] = 1.0f / tans[k];
						least = b3MinFloat( least, tans[k] );
					}

					// sin(a / 2) = 2 t / (1 + t^2) for t = tan(a / 4)
					float sine = 2.0f * least / ( 1.0f + least * least );
					bone->swingInside = sine * sine;
				}
#endif
				break;
			}
			default:
			{
				b3WeldJointDef jointDef = b3DefaultWeldJointDef();
				jointDef.base.bodyIdA = parentId;
				jointDef.base.bodyIdB = bone->bodyId;
				jointDef.base.localFrameA = frameA;
				jointDef.base.localFrameB = frameB;
				jointDef.base.collideConnected = false;
				jointDef.base.constraintDampingRatio = NB_JOINT_DAMPING_RATIO;
				bone->jointId = b3CreateWeldJoint( world->physicsWorld, &jointDef );
				break;
			}
		}
	}

	// How the upper body of the humanoid turns about its long axis against how it tilts, from the shapes of its bones
	nbComputeTwist( f );

	// The humanoid catches itself after a hard hit, see figure_balance.c, and falls when its center of mass sinks too far
	// below where it stands, in the first pose of its idle clip. Up to active-ragdoll-37 the height came from the bones
	// at rest, with straight legs, and from their pivots, as Box3D gives kinematic bodies no center of mass: 1.02 m for
	// the humanoid, whose center of mass stands at 0.94 m.
	f->canBalance = nbCanBalance( f );
	if ( f->canBalance )
	{
		b3Transform standing[nb_humanoidBoneCount];
		nbFramePose( f, f->root, f->pose, standing );
		nbInitBalancer( f, nbFrameCenter( f, standing ).y );
	}

	if ( def->startAsRagdoll )
	{
		nbFall( world, f, false );
		f->knockdownCount = 0;
	}

	return nbMakeFigureId( world, index );
}

static void nbDestroyFigureAt( nbWorld* world, int index )
{
	nbFigure* f = world->figures.data + index;
	bool physicsAlive = b3World_IsValid( world->physicsWorld );

	// A body takes its shapes and joints with it
	for ( int i = 0; i < f->boneCount; ++i )
	{
		nbUnmapBone( world, f->bones[i].bodyId );
		if ( physicsAlive && b3Body_IsValid( f->bones[i].bodyId ) )
		{
			b3DestroyBody( f->bones[i].bodyId );
		}
	}

	for ( int k = 0; k < nb_figureClipCount; ++k )
	{
		nbFreeClip( f->ownClips + k );
	}
	nbFreeClip( &f->playClip );
	nbFreeGaitClips( f );
	size_t boneCount = (size_t)f->boneCount;
	nbFree( f->bones, sizeof( nbBone ) * boneCount );
	nbFree( f->pose, sizeof( b3Quat ) * boneCount );
	nbFree( f->fadePose, sizeof( b3Quat ) * boneCount );
	nbFree( f->lyingPose, sizeof( b3Quat ) * boneCount );
	nbFree( f->targets, sizeof( b3WorldTransform ) * boneCount );
	if ( f->followJoints != NULL )
	{
		nbFree( f->followJoints, sizeof( b3JointId ) * boneCount );
		nbFree( f->followTargets, sizeof( b3WorldTransform ) * boneCount );
		nbFree( f->followVelocities, sizeof( b3Vec3 ) * boneCount );
		nbFree( f->followRelative, sizeof( b3Quat ) * boneCount );
	}
	uint16_t generation = f->generation;
	memset( f, 0, sizeof( nbFigure ) );
	f->generation = (uint16_t)( generation + 1 );
	f->isFree = true;
	nbArray_Push( world->freeFigures, index );
}

void nbDestroyFigure( nbFigureId figureId )
{
	nbWorld* world;
	nbFigure* f = nbGetFigureFromId( figureId, &world );
	if ( f == NULL )
	{
		return;
	}
	nbDestroyFigureAt( world, (int)( f - world->figures.data ) );
}

void nbDestroyFigures( nbWorld* world )
{
	for ( int i = 0; i < world->figures.count; ++i )
	{
		if ( world->figures.data[i].isFree == false )
		{
			nbDestroyFigureAt( world, i );
		}
	}
	nbArray_Free( world->figures );
	nbArray_Free( world->freeFigures );
	nbArray_Free( world->bodyToBone );
	nbArray_Free( world->figurePelvises );
	if ( b3World_IsValid( world->physicsWorld ) && b3Body_IsValid( world->figureAnchor ) )
	{
		b3DestroyBody( world->figureAnchor );
	}
	world->figureAnchor = b3_nullBodyId;

	for ( int i = 0; i < world->humanoidClips.count; ++i )
	{
		nbClipSet* set = world->humanoidClips.data[i];
		for ( int k = 0; k < nb_figureClipCount; ++k )
		{
			nbFreeClip( set->clips + k );
		}
		nbFree( set, sizeof( nbClipSet ) );
	}
	nbArray_Free( world->humanoidClips );
}

//----------------------------------------------------------------------------------------------------------------------
// Figures

bool nbFigure_IsValid( nbFigureId figureId )
{
	return nbGetFigureFromId( figureId, NULL ) != NULL;
}

int nbFigure_GetBoneCount( nbFigureId figureId )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	return f != NULL ? f->boneCount : 0;
}

b3BodyId nbFigure_GetBody( nbFigureId figureId, int bone )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL || bone < 0 || bone >= f->boneCount )
	{
		return b3_nullBodyId;
	}
	return f->bones[bone].bodyId;
}

b3JointId nbFigure_GetJoint( nbFigureId figureId, int bone )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL || bone < 1 || bone >= f->boneCount )
	{
		return b3_nullJointId;
	}
	return f->bones[bone].jointId;
}

nbFigureState nbFigure_GetState( nbFigureId figureId )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	return f != NULL ? f->state : nb_figureAnimated;
}

nbFigureInfo nbFigure_GetInfo( nbFigureId figureId )
{
	nbFigureInfo info = { 0 };
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL )
	{
		return info;
	}

	info.state = f->state;
	info.stateTime = f->stateTime;
	info.strength = f->state == nb_figureRagdoll || f->state == nb_figureBalancing ? f->strength : 0.0f;
	info.protect = f->state == nb_figureRagdoll ? f->protect : 0.0f;
	info.brace = f->state == nb_figureRagdoll ? f->brace : 0.0f;
	info.moveVelocity = f->steered && f->state == nb_figureAnimated ? f->moveVelocity : b3Vec3_zero;
	info.onGround = f->steered && f->state == nb_figureAnimated && f->onGround;
	info.speed = f->speed;
	info.restTime = f->state == nb_figureRagdoll ? f->restTime : 0.0f;
	info.onBack = f->onBack;
	info.wait = f->state == nb_figureRagdoll ? f->wait : nb_waitNone;
	info.getUpTime = f->state == nb_figureGettingUp ? f->getUpTime : 0.0f;
	info.getUpBlend = f->state == nb_figureGettingUp ? b3ClampFloat( f->getUpTime / NB_GET_UP_BLEND, 0.0f, 1.0f ) : 0.0f;
	info.flinch = b3RotateVector( f->frame.q, f->flinch );
	info.stepping = ( f->state == nb_figureAnimated && f->balancing ) || nbBalanceStepping( f );
	info.shifting = f->state == nb_figureGettingUp && f->shiftLeft > 0.0f;
	info.following = f->following;
	info.hitCount = f->hitCount;
	info.knockdownCount = f->knockdownCount;
	info.getUpCount = f->getUpCount;
	info.abortCount = f->abortCount;
	info.stepCount = f->stepCount;
	info.shiftCount = f->shiftCount;
	info.catchCount = f->catchCount;
	info.pushCount = f->pushCount;
	info.letGoCount = f->letGoCount;
	return info;
}

void* nbFigure_GetUserData( nbFigureId figureId )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	return f != NULL ? f->def.userData : NULL;
}

b3WorldTransform nbFigure_GetFrame( nbFigureId figureId )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	return f != NULL ? f->frame : (b3WorldTransform){ b3ToPos( b3Vec3_zero ), b3Quat_identity };
}

void nbFigure_Move( nbFigureId figureId, b3Pos position, float heading )
{
	nbWorld* world;
	nbFigure* f = nbGetFigureFromId( figureId, &world );
	if ( f == NULL || f->state != nb_figureAnimated )
	{
		return;
	}

	// The program takes over where the figure stands, a step to catch a hit ends and the feet go where the clip puts them
	bool far = b3Length( b3SubPos( position, f->frame.p ) ) > 1.0f;
	f->heading = heading;
	f->frame = nbMakeFrame( world, position, heading );
	f->balancing = false;
	f->swingFoot = -1;
	f->footWeight = 0.0f;
	if ( f->steered )
	{
		nbResetSteering( f );
	}
	if ( far )
	{
		nbResetFeet( f );
	}
	if ( far )
	{
		// A jump: the bones go there at once, without the velocity of a jump, kinematic again
		nbEndFollowing( world, f, true );
		nbComputePose( f, b3MulWorldTransforms( f->frame, f->root ), f->pose, f->targets );
		for ( int i = 0; i < f->boneCount; ++i )
		{
			b3Body_SetTransform( f->bones[i].bodyId, f->targets[i].p, f->targets[i].q );
			b3Body_SetLinearVelocity( f->bones[i].bodyId, b3Vec3_zero );
			b3Body_SetAngularVelocity( f->bones[i].bodyId, b3Vec3_zero );
		}
	}
}

void nbFigure_Steer( nbFigureId figureId, const nbSteer* steer )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL || f->steered == false || steer == NULL )
	{
		return;
	}
	f->steer = *steer;
}

void nbFigure_PlayClip( nbFigureId figureId, const nbClipDef* clip, float speed )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL )
	{
		return;
	}

	nbCopyClip( &f->playClip, clip, f->boneCount );
	f->playing = f->playClip.keyframeCount > 0;
	f->clipTime = 0.0f;
	f->clipSpeed = speed;
	if ( f->state == nb_figureAnimated )
	{
		nbStartFade( f, NB_CLIP_FADE );
	}
}

void nbFigure_SetGaitClips( nbFigureId figureId, const nbGaitClipDef* clips, int count )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL )
	{
		return;
	}

	bool had = f->gaitClipCount > 0;
	nbCopyGaitClips( f, clips, count );
	f->gaitRoot = b3Transform_identity;
	if ( f->state == nb_figureAnimated )
	{
		// Taken away, its own gait comes in as the body blends from the clips, so its arms do not swing twice
		nbStartFade( f, NB_CLIP_FADE );
		f->fadeFromClips = had && f->gaitClipCount == 0;
		f->gaitBlend = f->fadeFromClips ? 0.0f : 1.0f;
	}
}

bool nbFigure_ApplyHit( nbFigureId figureId, const nbHitDef* hit )
{
	nbWorld* world;
	nbFigure* f = nbGetFigureFromId( figureId, &world );
	if ( f == NULL || hit == NULL || hit->bone < 0 || hit->bone >= f->boneCount )
	{
		return false;
	}
	if ( nbTakeHitId( f, hit->id ) == false )
	{
		return false;
	}

	if ( f->state == nb_figureRagdoll )
	{
		// A hit that meets a ragdoll acts as it is and leaves it slack a moment, as Overgrowth's limp stun, see
		// nbFigureMotors::stunTime
		f->hitCount += 1;
		f->stun = b3MaxFloat( f->stun, f->def.motors.stunTime );
		nbHitBones( f, hit->bone, hit->impulse, hit->point );
		return true;
	}

	nbTakeHit( world, f, hit->bone, hit->impulse, hit->point, false );
	return true;
}

void nbFigure_Knockdown( nbFigureId figureId )
{
	nbWorld* world;
	nbFigure* f = nbGetFigureFromId( figureId, &world );
	if ( f != NULL && f->state != nb_figureRagdoll )
	{
		// A figure that catches itself lets go of its upright hold, see nbFall
		nbFall( world, f, false );
	}
}

int nbFigure_GetBoneTransforms( nbFigureId figureId, b3WorldTransform* transforms, int capacity )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL || transforms == NULL )
	{
		return 0;
	}

	int count = b3MinInt( capacity, f->boneCount );
	for ( int i = 0; i < count; ++i )
	{
		transforms[i] = b3Body_GetTransform( f->bones[i].bodyId );
	}
	return count;
}

int nbFigure_GetTargetTransforms( nbFigureId figureId, b3WorldTransform* transforms, int capacity )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL || transforms == NULL )
	{
		return 0;
	}

	int count = b3MinInt( capacity, f->boneCount );
	if ( f->state == nb_figureRagdoll || f->state == nb_figureBalancing )
	{
		// The pose the joints pull toward, on the root as it is
		b3WorldTransform* pose = nbAlloc( sizeof( b3WorldTransform ) * (size_t)f->boneCount );
		b3Quat* limited = nbAlloc( sizeof( b3Quat ) * (size_t)f->boneCount );
		for ( int i = 0; i < f->boneCount; ++i )
		{
			limited[i] = nbLimitBoneRotation( f->bones + i, f->pose[i] );
		}
		nbComputePose( f, b3Body_GetTransform( f->bones[0].bodyId ), limited, pose );
		memcpy( transforms, pose, sizeof( b3WorldTransform ) * (size_t)count );
		nbFree( limited, sizeof( b3Quat ) * (size_t)f->boneCount );
		nbFree( pose, sizeof( b3WorldTransform ) * (size_t)f->boneCount );
		return count;
	}

	memcpy( transforms, f->targets, sizeof( b3WorldTransform ) * (size_t)count );
	return count;
}

nbFigureId nbWorld_GetFigureFromBody( nbWorldId worldId, b3BodyId bodyId, int* bone )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return nb_nullFigureId;
	}

	int index = nbFigureOfBody( world, bodyId, bone );
	return index != NB_NULL_INDEX ? nbMakeFigureId( world, index ) : nb_nullFigureId;
}

// A ragdoll whose bones are awake in Box3D, see nbFigureStats::awakeRagdollCount
static bool nbIsAwakeRagdoll( const nbFigure* f )
{
	return f->isFree == false && ( f->state == nb_figureRagdoll || f->state == nb_figureBalancing || f->following ) &&
		   b3Body_IsAwake( f->bones[0].bodyId );
}

int nbCountAwakeRagdolls( const nbWorld* world )
{
	int count = 0;
	for ( int i = 0; i < world->figures.count; ++i )
	{
		count += nbIsAwakeRagdoll( world->figures.data + i ) ? 1 : 0;
	}
	return count;
}

nbFigureStats nbWorld_GetFigureStats( nbWorldId worldId )
{
	nbFigureStats stats = { 0 };
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return stats;
	}

	for ( int i = 0; i < world->figures.count; ++i )
	{
		const nbFigure* f = world->figures.data + i;
		if ( f->isFree )
		{
			continue;
		}

		stats.figureCount += 1;
		stats.animatedCount += f->state == nb_figureAnimated ? 1 : 0;
		stats.ragdollCount += f->state == nb_figureRagdoll ? 1 : 0;
		stats.gettingUpCount += f->state == nb_figureGettingUp ? 1 : 0;
		stats.balancingCount += f->state == nb_figureBalancing ? 1 : 0;
		stats.awakeRagdollCount += nbIsAwakeRagdoll( f ) ? 1 : 0;
		stats.followingCount += f->following ? 1 : 0;
		stats.hitCount += f->hitCount;
		stats.knockdownCount += f->knockdownCount;
		stats.getUpCount += f->getUpCount;
		stats.abortCount += f->abortCount;
		stats.stepCount += f->stepCount;
		stats.shiftCount += f->shiftCount;
		stats.catchCount += f->catchCount;
		stats.pushCount += f->pushCount;
		stats.letGoCount += f->letGoCount;
	}
	return stats;
}
