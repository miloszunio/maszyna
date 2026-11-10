// Our own members of editor_mode (ficzery2), included inside the class, in its private part: the field of work of the features
// (F7), the road profile and the structures under the tracks, the hourglass of the heavy work. Kept apart so the merges of the
// others' work touch editormode.h less. No include guard: it is a part of the class, included once.

	// the outlined area cut out of the terrain with its heights kept, as a separate surface with the chosen material
	void cut_out_surface();
	void render_patches_ui();
	// the outline not only of straight sides (editormodeoutline.cpp): each option on or off by itself
	struct outline_shape
	{
		bool round{false}; // the corners rounded off
		float radius{5.0f};
		bool spline{false}; // a smooth curve through the points, Ctrl+click a sharp corner
		float tension{0.5f};
		bool bezier{false}; // handles dragged out as a point is put
		bool freehand{false}; // drawn with the button held
		float freehand_step{1.0f};
		float freehand_tolerance{0.3f};
		bool along{false}; // the sides between points by a track follow it at a distance of its axis
		bool along_from_click{true};
		float along_offset{10.0f};
	};
	struct outline_mark
	{
		glm::dvec3 position{0.0}; // of the point it's of, to keep the marks with their points
		bool sharp{false};
		bool follow{false}; // the side to the next point along the track
		bool freehand{false};
		bool on_track{false};
		glm::dvec2 handle{0.0};
		double offset{0.0}; // from the axis of the track, positive to its left
	};
	outline_shape m_outline_shape;
	mutable std::vector<outline_mark> m_outline_marks;
	bool m_outline_pressed{false};
	std::size_t m_outline_press_index{0};
	std::vector<glm::dvec2> m_outline_stroke;
	mutable std::string m_outline_shaped_key;
	mutable std::vector<glm::dvec2> m_outline_shaped;
	std::vector<outline_mark> &outline_marks() const;
	void add_outline_point(glm::dvec3 const &World);
	void outline_drag(bool Held);
	std::string outline_key() const;
	// the outline as it's cut or filled: the curves made of the points clicked
	std::vector<glm::dvec2> const &shaped_outline() const;
	std::string outline_controls() const;
	bool restore_outline(std::string const &Controls);
	void render_outline_options();
	void draw_outline_shape(ImDrawList *Drawlist);
	// the field of work of our own features (editormodefeatures.cpp): surfaces cut out of the terrain to texture them, and the
	// track bed and the slopes along the tracks cut into pieces of their own
	enum class feature_tool
	{
		cut_out, // an outline drawn as with Fill, painted into the terrain as a surface of its own
		track_bed, // the track bed, embankments and cuttings along the formation modifiers, in pieces up to 100 m
		go_to, // the map with the camera on it, a search of towns and the places of the camera kept
		zielsko // ZIELSKO z kosmosu: the trees of the 3D tree models of geoportal.gov.pl planted along the tracks
	};
	// ZIELSKO z kosmosu (editormodezielsko.cpp, editor/editorZielsko.cpp)
	struct zielsko_settings
	{
		float reach{300.0f}; // m from a rail track: the trees kept of the counties read
		float nearest{8.0f}; // m from the axis of a rail track
		float farthest{150.0f};
		float road{2.0f}; // m off the edge of a road
		float lowest{4.0f};
		float spacing{0.0f};
		int limit{20000};
		int scope{0}; // 0 around the camera, 1 the whole scenery, 2 within the outline
		float radius{1500.0f};
		char trees_group[64]{"Duze drzewka"};
		char low_group[64]{"Krzaki"};
		float small_below{8.0f};
		float model_height{0.0f}; // 0: from the model
		float jitter{0.08f};
		bool again{false};
	};
	struct zielsko_candidate
	{
		glm::dvec2 at; // x, z of the scene
		float height;
		float track; // m from the nearest rail track
	};
	zielsko_settings m_zielsko;
	zielsko::harvest m_zielsko_harvest;
	std::vector<zielsko_candidate> m_zielsko_preview;
	std::string m_zielsko_status;
	std::string zielsko_folder() const;
	void zielsko_harvest_start();
	std::vector<zielsko_candidate> zielsko_candidates(std::size_t &Counties, std::size_t &Read);
	void zielsko_count();
	void zielsko_plant();
	std::size_t zielsko_remove();
	void draw_zielsko_preview(ImDrawList *Drawlist);
	void render_zielsko_ui();
	// the transition the surfaces cut out blend over, and the noise their edge is shifted by
	float m_cut_feather{1.0f};
	float m_cut_noise{0.3f};
	float m_cut_noise_scale{4.0f};
	// the ground under a surface cut out made wavy: rolling hills, furrows of a field, bumps, long waves, a hollow or a mound
	struct terrain_waves
	{
		enum kind { hills, furrows, bumps, waves, hollow, grid, strips, count };
		// the profile of a wave: rounded as a sine, sharp ridges, sharp valleys, terraces, flat tops; how sharp it goes
		enum profile { smooth, ridges, valleys, terraces, plateaus, profiles };
		bool on{false};
		int kind{hills};
		float height{0.4f}; // metres, from the lowest to the highest point of a wave; negative for the hollow, upside down
		float length{25.0f}; // metres, of a wave
		float angle{0.0f}; // degrees the furrows and the waves run at, counterclockwise from x
		float edge{6.0f}; // metres inside the outline over which the waves fade in from nothing
		float variation{0.35f}; // 0..1, how much the waves differ from one another
		int shape{smooth};
		float sharpness{0.5f}; // 0..1: 0 the profile still rounded, 1 its corners and edges as sharp as it gets
		int steps{4}; // of the terraces, from the bottom of a wave to its top
		float roughness{0.0f}; // 0..1, small bumps over the whole surface, as clods of a field
	};
	terrain_waves m_waves;
	void render_waves_options();
	// the ground within Outline made wavy as m_waves says; returns true if it changed
	bool apply_waves(std::vector<glm::dvec2> const &Outline);
	// the 3D grass under the surfaces painted: -2 left, -1 taken away, 0 or more: that kind grown (heightmap::surface_patch::grass)
	int m_cut_grass{-1};
	bool m_cut_separate{false}; // the Fields: a mesh of its own over a hole instead of the paint of the chunks
	void render_fields_ui();
	void render_natural_slopes();
	void render_cut_surfaces();
	// a combo of what's done with the grass; true if changed
	bool grass_choice(char const *Label, int &Choice);
	feature_tool m_feature_tool{feature_tool::track_bed};
	bool m_features_open{false};
	struct track_bed_settings
	{
		float length{100.0f}; // the longest piece along the axis
		float step{5.0f}; // between the points of the outlines along the axis
		float reach{60.0f}; // the farthest a slope goes from the edge of the track bed
		float tolerance{0.15f}; // how far off the slope of the modifier the ground may be, still the slope
		float least_slope{0.5f}; // a narrower slope isn't made a piece of its own
		float radius{1000.0f}; // around the camera
		bool everywhere{false}; // all the modifiers of the terrain
		float bed_feather{0.5f}; // the transition of the track bed into the slopes
		float slope_feather{1.5f}; // the transition of the slopes into the ground
		int bed_grass{-1}; // the grass on the track bed taken away
		int slope_grass{-2}; // and on the slopes left
	};
	// the surfaces painted into the terrain: the material of one, with its transition and noise; true if anything was painted
	bool paint_patch(heightmap::surface_patch const &Patch);
	// the list given to the terrain, the changed ones painted anew and then the ones over them that they touch (the track bed over
	// the slopes, the surfaces drawn by hand over both), so the transitions are made again; the Cleared outlines painted first
	// with the first material of the palette, as the ground was
	void repaint_patches(std::vector<heightmap::surface_patch> All, std::vector<std::size_t> const &Changed, std::vector<heightmap::surface_patch> const &Cleared = {});
	// what a cab-view film saw beside the track (editormodefilm.cpp): the .meta of kabina_meta.py laid along the route of the film over
	// the tracks of the scenery (.trasa), the dry grass, the undergrowth and the gravel as surfaces painted into the terrain, the
	// trees and the bushes as models of the node bank; named "kabina ..." and "kabina_<n>", so they're found and taken away again
	struct film_settings
	{
		char meta[260]{"filmy/wroclaw.meta"};
		char route[260]{"filmy/wroclaw.trasa"};
		char dry[96]{"grasbrown2"};
		char undergrowth[96]{"grassdarkgreen5"};
		char gravel[96]{""}; // the ballast the film sees further off is mostly the next track: not painted unless asked
		char trees[64]{"Duze drzewka"};
		char bushes[64]{"Krzaki"};
		float forest{350.0f}; // trees a hectare where the film saw a wood all over
		float shrubs{250.0f};
		float nearest{5.5f}; // m from the axis: the clearance and the poles
		float shift{4.0f}; // m the trees and bushes are moved off the track: the film sees their crowns, which stand nearer than their trunks
		float bed{3.2f}; // m from the axis, the track bed of the scenery: not painted
		float shortest{25.0f};
		float feather{3.0f};
		float noise{1.5f};
		float noise_scale{8.0f};
		bool surfaces{true};
		bool plants{true};
	};
	film_settings m_film;
	std::string m_film_status;
	void render_film_ui();
	void lay_out_film();
	void remove_film();
	// the magic wand: a click on the orthophoto picks the area of about the colour clicked, which becomes the outline
	struct wand_settings
	{
		bool on{false};
		float tolerance{18.0f}; // 0..100 of the difference of the colours
		float radius{200.0f}; // m, half the side of the square read around the click
		float detail{0.5f}; // m a pixel
		float simplify{0.75f}; // m, points nearer a straight side dropped
		bool holes{true};
	};
	wand_settings m_wand;
	std::string m_wand_status;
	bool wand_select(glm::dvec3 const &World);
	// Ctrl+Tab: the tiles of the orthophoto the camera sees asked for before any other
	void orthophoto_hurry();
	// around the camera or the whole scenery, beside every button which works over an area
	void render_scope(char const *Id);
	// the paint of the whole terrain taken away, pack by pack over the frames (each chunk covered with the first material of the
	// palette alone), after a copy of the terrain files is taken
	struct paint_clearing
	{
		std::vector<std::pair<int, int>> packs; // still to do
		std::size_t total{0};
		std::size_t chunks{0}; // with more than one material, cleared
		std::string backup;
	};
	paint_clearing m_paint_clearing;
	void clear_paint_start();
	void clear_paint_step();
	void render_wand_options();
	// the surfaces with a mesh and a hole of their own (made before the painting came) painted into the terrain instead
	void convert_patches_to_paint();
	// the Go to tool: the map with the camera, the search of the towns and the places of the camera kept with the scenery
	editor_map m_goto_map;
	bool m_goto_follow{true};
	char m_goto_query[96]{};
	int m_goto_found{-1};
	struct camera_place
	{
		std::string name;
		glm::dvec3 position{0.0};
		glm::dvec3 angle{0.0};
	};
	std::vector<camera_place> m_camera_places;
	std::string m_camera_places_of; // the scenery they were read for
	char m_camera_place_name[64]{};
	void render_go_to_ui();
	// the origin of the scenery kept with the terrain (terrain.yaml): a terrain which knows where it lies gives the origin to the
	// orthophoto, the terrain model and the maps, whatever was typed or clicked before; one which doesn't gets the origin of the
	// //$g line of the scenery. Checked every frame, acted on once a terrain
	void sync_terrain_origin();
	bool m_terrain_origin_synced{false};
	// the place of the scenery may be moved although the terrain knows where it lies (NMT, Location)
	bool m_origin_unlocked{false};
	void read_camera_places();
	void write_camera_places() const;
	track_bed_settings m_track_bed;
	void render_features_ui();
	void render_material_palette();
	void render_palette_entries();
	// the palette as tiles in groups: the group of an entry, and setting it (kept beside the terrain)
	std::string palette_group(std::size_t Layer);
	void palette_group(std::size_t Layer, std::string const &Group);
	std::map<int, std::string> m_palette_groups;
	std::string m_palette_groups_dir;
	int m_paint_mode{0}; // the brush: 0 mixes the material in, 1 covers with it fully, 2 takes the paint back to the first material
	bool paint_at_cursor(std::vector<std::pair<std::uint16_t, int>> &Weights, std::size_t &Used) const; // the entries of the palette of the terrain: picked, moved up and down, taken off
	void cut_track_bed();
	// the slopes of the formation modifiers around the camera (or of all) made natural, the terrain shaped anew
	heightmap::modifier::natural_shape m_natural{0.8, 15.0, 0.15, 0.3, 0.08};
	void naturalise_slopes(bool Natural);

	// the heights of the selected road (all its pieces) evened out: grades no steeper than set, crests and dips rounded with
	// the radii set, the ends where it meets other roads kept where they are
	void road_profile_smooth();
	// the terrain shaped to the road with a modifier, the way the formation of a track does: under the roadway and its shoulders
	// flat at the surface, with slopes 1:n to the ground beside; of the selected road or of all of them
	void road_terrain(bool Remove);
	// at every crossing of the selected road with the formation of a track (a modifier of the terrain along it), where the road
	// runs under it: an arch, a frame viaduct or a box culvert with its faces along the track and the wing walls along the slopes,
	// the ground before the faces cut away; written as models of their own with the outlines of their holes beside them.
	// Remove: the ones of the road taken away and their holes closed
	void road_structures(bool Remove);
	// the textures of walls under textures/, to choose the one of the structures from
	void structure_textures_search();
	// the vertical profile of the selected road in a window of its own (P, or the button of the road panel): the ground and the
	// road along it, and what evens it out, leads the terrain to it and builds the structures under the tracks
	struct road_profile_view
	{
		bool open{false};
		road_node *road{nullptr}; // the profile shown is of the road this piece belongs to
		std::vector<float> runs; // m from the start of the road, seen from above
		std::vector<float> heights; // of the road
		std::vector<float> ground_runs; // where the ground was looked up
		std::vector<float> ground; // under it, NaN where there's none
		// the nodes: the points where the bezier parts of the road meet or end, in order along it
		std::vector<float> node_runs;
		std::vector<float> node_heights;
		std::vector<glm::dvec3> node_positions;
		std::vector<char> selected; // of the nodes
		std::vector<glm::dvec3> synced; // the points of the road tool as they were last matched to the nodes
		std::size_t history{0}; // length of the undo history the profile was read at
		int exaggeration{4}; // index of the stretch of the heights: fit, 1, 2, 5, 10, 20, 50
		bool dragging{false}; // nodes dragged up or down in the graph
		float drag_mouse{0.0f};
		float drag_scale{0.0f}; // metres for a pixel of the graph
		bool boxing{false}; // a stretch being boxed in the graph
		ImVec2 box_from{0.0f, 0.0f};
		// what's made of the selected nodes: 0 level, 1 one grade, 2 a vertical curve, 3 over the ground, 4 up or down by, 5 evened out
		int op{0};
		float level{0.0f}; // m
		float grade{2.0f}; // %
		bool grade_ends{true}; // the grade from the first selected node to the last, not the one typed in
		float radius{2000.0f}; // m, of the vertical curve
		float offset{0.3f}; // m over the ground
		float shift{-0.5f}; // m up or down
		int road_class{2};
	};
	road_profile_view m_road_profile;
	void road_profile_window_open();
	void road_profile_window_toggle();
	void road_profile_sample();
	void road_profile_select_points();
	// the heights and the grades of the nodes of the road written into its pieces, as one step of the history
	bool road_profile_commit(road_node *Road, std::vector<double> const &Heights, std::vector<double> const &Grades);
	// what's chosen in the window done to the selected nodes
	void road_profile_operation();
	void render_road_profile_window();

	// the heavy work of a tool: the screen frozen with an hourglass and the name of it over the view first, then the work
	// done at the start of the next frame, so the hourglass is what's seen until it's over
	struct busy_state
	{
		bool pending{false};
		int shown{0}; // frames the hourglass was drawn in
		std::string task;
		std::function<void()> work;
	};
	busy_state m_busy;
public:
	void run_busy(std::string Task, std::function<void()> Work);
private:
	void busy_run_pending();
	void draw_busy_overlay();

	// in the Fill tool of the terrain: one material for another, within the outline only
	void render_replace_in_outline();
