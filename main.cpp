#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include <optional>
#include <future>
#include <variant>

using namespace std::string_literals;

namespace game
{
	enum class EPiece
	{
		None = 0,
		Cross = 1,
		Cricle = 2
	};

	// Constants
	constexpr int tileSize = 32;
	constexpr int pixelSize = 2;
	constexpr int boardWidth = 3; // values greater than 3 will not work with ai cause its too slow
	constexpr int piecesToWin = 3;
	constexpr bool playerStart = true;
	constexpr bool useAi = true;
	constexpr float aiThinkTime = 0.5f;

	static_assert(!useAi || (boardWidth < 4), "AI and board size > 3 is disabled");

	constexpr EPiece playerPiece = playerStart ? EPiece::Cross : EPiece::Cricle;
	constexpr EPiece computerPiece = playerStart ? EPiece::Cricle : EPiece::Cross;

	// Typedefs
	using Board = std::array<EPiece, boardWidth* boardWidth>;
	using Sprite = std::shared_ptr<olc::Sprite>;
	using Renderable = std::variant<std::monostate, olc::Pixel, Sprite>;

	struct WinningMove
	{
		EPiece piece;
		olc::vi2d position;
		olc::vi2d direction;
	};

	// Helpers for variant access
	template<class... Ts> struct make_visitor : Ts... { using Ts::operator()...; };
	template<class... Ts> make_visitor(Ts...)->make_visitor<Ts...>;

	// Returns information of winner if there is any
	[[nodiscard]] std::optional<WinningMove> CheckWin(const Board& board, int placedPiecePosition);
	// Checks a line from the position in a given direction, returns the amount of pieces in row found
	[[nodiscard]] int CheckLine(const Board& board, EPiece expected, olc::vi2d pos, olc::vi2d searchDirection);

	// Find Best move on a board
	[[nodiscard]] int FindBestMove(Board board, EPiece piece);
	// Minimax algorithm used by FindBestMove
	[[nodiscard]] int MiniMax(Board& board, int depth, int placedPiece, bool isMax);

	class App : public olc::PixelGameEngine
	{
	public:
		App()
		{
			sAppName = "tic tac toe"s;
		}

	public:
		bool OnUserCreate() override
		{
			PieceToRenderable.emplace(EPiece::None, olc::BLACK);
			PieceToRenderable.emplace(EPiece::Cross, std::make_shared<olc::Sprite>("cross.png"s));
			PieceToRenderable.emplace(EPiece::Cricle, std::make_shared<olc::Sprite>("circle.png"s));

			Reset();
			return true;
		}

		// Game loop
		bool OnUserUpdate(float fElapsedTime) override
		{
			DrawBoard();
			DrawBoardLines();
			HighlightSelected(GetMousePos());

			if (bGameEnded)
			{
				restartTimer += fElapsedTime;
				if (restartTimer > 2.0f)
				{
					Reset();
					return true;
				}
				if (winningMove)
				{
					DrawWinningLine(winningMove.value());
				}

				if(endMessage.length() > 0)
				{
					FillRect({ 0, 0 }, {boardWidth*tileSize, tileSize / 2}, olc::BLACK);
					DrawString({ 0, tileSize/6 }, endMessage);
				}

				return true;
			}

			if (useAi && currentTurn == computerPiece)
			{
				aiThinkAccumulate += fElapsedTime;
				HandleAiTurn();
			}
			else
			{
				HandlePlayerTurn();
			}

			if (winningMove)
			{
				bGameEnded = true;
				if (useAi && winningMove->piece == computerPiece)
				{
					std::cout << "Computer won!"s << std::endl;
					endMessage = "Computer won!"s;
				}
				else
				{
					std::string message = (winningMove->piece == EPiece::Cross ? "Crosses win!"s : "Circles win!"s);
					endMessage = message;
					std::cout << message << std::endl;
				}
			}
			else if (placedPieces >= boardWidth * boardWidth)
			{
				bGameEnded = true;
				endMessage = "It's a draw :/"s;
				std::cout << "Its a draw :S"s << std::endl;
			}

			return true;
		}

	private:
		Board board{};
		int placedPieces = 0;
		EPiece currentTurn = EPiece::None;

		float aiThinkAccumulate = 0.0f;
		std::future<int> aiNextmMove;

		bool bGameEnded = false;
		float restartTimer = 0.0f;
		std::string endMessage;

		std::optional<WinningMove> winningMove{};
		std::map<EPiece, Renderable> PieceToRenderable;

	private:
		void HighlightSelected(olc::vi2d mousePos)
		{
			const auto SelectedTile = WindowPosToBoardIdx(GetMousePos());

			const int x = SelectedTile / boardWidth;
			const int y = SelectedTile % boardWidth;
			const int squaresize = tileSize - 4;
			

			if (SelectedTile >= 0 && SelectedTile < board.size())
			{

				DrawRect({ x * tileSize+2, y * tileSize+2 },
					{ squaresize, squaresize }, olc::YELLOW);
			}
		}

		void StartAiThink()
		{
			aiThinkAccumulate = 0.0f;
			aiNextmMove = std::async(std::launch::async, FindBestMove, board, computerPiece);
		}

		void HandleAiTurn()
		{
			if (aiThinkAccumulate > aiThinkTime)
			{
				auto status = aiNextmMove.wait_for(std::chrono::milliseconds(10));
				if (status == std::future_status::timeout)
				{
					std::cout << "waiting for ai" << std::endl;
					return;
				}
				auto move = aiNextmMove.get();
				if (move != -1)
				{
					if (board.at(move) == EPiece::None)
					{
						placedPieces++;
						board.at(move) = computerPiece;
						currentTurn = currentTurn == EPiece::Cross ? EPiece::Cricle : EPiece::Cross;
						winningMove = CheckWin(board, move);
					}
				}
			}
		}

		void HandlePlayerTurn()
		{
			if (GetMouse(0).bPressed)
			{
				const auto SelectedTile = WindowPosToBoardIdx(GetMousePos());

				if (board.at(SelectedTile) == EPiece::None)
				{
					placedPieces++;
					board.at(SelectedTile) = currentTurn;
					currentTurn = currentTurn == EPiece::Cross ? EPiece::Cricle : EPiece::Cross;
					winningMove = CheckWin(board, SelectedTile);
					if (useAi)
					{
						StartAiThink();
					}
				}
			}
		}

		void DrawBoardLines()
		{
			for (int x = 0; x < boardWidth; x++)
			{
				DrawLine({ 0, x * tileSize }, { ScreenWidth(), x * tileSize });
			}
			for (int y = 0; y < boardWidth; y++)
			{
				DrawLine({ y * tileSize, 0 }, { y * tileSize, ScreenHeight() });
			}
		}

		void DrawWinningLine(const WinningMove& wm)
		{
			olc::vi2d start = wm.position;
			bool reachedStart = false;
			while (!reachedStart)
			{
				const olc::vi2d check = start + wm.direction;
				if (check.x < 0 || check.y < 0)
				{
					reachedStart = true;
				}
				else if (board.at(check.x * boardWidth + check.y) != wm.piece)
				{
					reachedStart = true;
				}
				else
				{
					start = check;
				}
			}

			const auto end = start + (-1 * wm.direction * piecesToWin);
			// If not diagonal winning move
			if (wm.direction.x == 0 || wm.direction.y == 0)
			{
				if (wm.direction.x != 0)
				{
					DrawLine({ start.x * tileSize , start.y * tileSize + tileSize / 2 }, 
						{ end.x * tileSize , end.y * tileSize + tileSize / 2 }, olc::YELLOW);
				}
				if (wm.direction.y != 0)
				{
					DrawLine({ start.x * tileSize + tileSize / 2 , start.y * tileSize }, 
						{ end.x * tileSize + tileSize / 2 , end.y * tileSize }, olc::YELLOW);
				}

			}
			else if (wm.direction.x != wm.direction.y)
			{
				DrawLine({ start.x * tileSize , (start.y + 1) * tileSize }, { end.x * tileSize , (end.y + 1) * tileSize }, olc::YELLOW);
			}
			else
			{
				DrawLine({ start.x * tileSize , start.y * tileSize }, { end.x * tileSize , end.y * tileSize }, olc::YELLOW);
			}
		}

		void DrawBoard()
		{
			for (int x = 0; x < boardWidth; x++)
			{
				for (int y = 0; y < boardWidth; y++)
				{
					const auto& renderable = PieceToRenderable.at(board.at(x * boardWidth + y));
					const auto vistor = make_visitor
					{
						[=](olc::Pixel p) { FillRect({ x * tileSize, y * tileSize },
										  { (x + 1) * tileSize, (y + 1) * tileSize },	p); },

						[=](Sprite s) {DrawSprite(x * tileSize, y * tileSize, s.get(), 1, 0); },

						[](auto) {std::cout << "bad variant access"; },
					};

					std::visit(vistor, renderable);
				}
			}
		}

		[[nodiscard]] int WindowPosToBoardIdx(const olc::vi2d& position) const noexcept
		{
			const int x = position.x / tileSize;
			const int y = position.y / tileSize;
			return x * boardWidth + y;
		}

		void Reset()
		{
			winningMove = {};
			restartTimer = 0.0f;
			bGameEnded = false;
			currentTurn = EPiece::Cross;
			board.fill(EPiece::None);
			placedPieces = 0;

			if (useAi && currentTurn == computerPiece)
			{
				StartAiThink();
			}
		}
	};

	// ----------------------------------------------------------------------------------------

	int CheckLine(const Board& board, EPiece expected, olc::vi2d pos, olc::vi2d searchDirection)
	{
		if (pos.x < 0 || pos.x >= boardWidth) { return 0; }
		if (pos.y < 0 || pos.y >= boardWidth) { return 0; }
		if (board.at(pos.x * boardWidth + pos.y) != expected) { return 0; }

		return 1 + CheckLine(board, expected, pos + searchDirection, searchDirection);
	}

	std::optional<WinningMove> CheckWin(const Board& board, int placedPiecePosition)
	{
		auto placedPiece = board.at(placedPiecePosition);
		const olc::vi2d pos = { placedPiecePosition / boardWidth, placedPiecePosition % boardWidth };

		int horizontalPieces = CheckLine(board, placedPiece, pos, { -1, 0 });
		horizontalPieces += CheckLine(board, placedPiece, { pos.x + 1, pos.y }, { 1, 0 });
		if (horizontalPieces >= piecesToWin)
		{
			const WinningMove wm = { placedPiece, pos, olc::vi2d(-1, 0) };
			return wm;
		}


		int verticalPieces = CheckLine(board, placedPiece, pos, { 0, -1 });
		verticalPieces += CheckLine(board, placedPiece, { pos.x, pos.y + 1 }, { 0, 1 });
		if (verticalPieces >= piecesToWin)
		{
			const WinningMove wm = { placedPiece, pos, olc::vi2d(0, -1) };
			return wm;
		}

		int diag1pieces = CheckLine(board, placedPiece, pos, { -1, -1 });
		diag1pieces += CheckLine(board, placedPiece, { pos.x + 1, pos.y + 1 }, { 1, 1 });
		if (diag1pieces >= piecesToWin)
		{
			const WinningMove wm = { placedPiece, pos, olc::vi2d(-1, -1) };
			return wm;
		}

		int diag2pieces = CheckLine(board, placedPiece, pos, { -1, 1 });
		diag2pieces += CheckLine(board, placedPiece, { pos.x + 1, pos.y - 1 }, { 1, -1 });
		if (diag2pieces >= piecesToWin)
		{
			const WinningMove wm = { placedPiece, pos, olc::vi2d(-1, 1) };
			return wm;
		}
		return {};
	}

	int MiniMax(Board& board, int depth, int placedPiece, bool isMax)
	{
		const auto winner = CheckWin(board, placedPiece);

		if (winner)
		{
			if (winner->piece == computerPiece)
			{
				return 1;
			}

			return -1;
		}

		bool hasEmpty = false;
		for (int i = 0; i < boardWidth * boardWidth; i++)
		{
			if (board.at(i) == EPiece::None)
			{
				hasEmpty = true;
				break;
			}
		}

		if (!hasEmpty)
		{
			return 0;
		}

		if (isMax)
		{
			int best = -1000;

			for (int i = 0; i < boardWidth * boardWidth; i++)
			{
				if (board.at(i) == EPiece::None)
				{
					board.at(i) = computerPiece;
					best = std::max<int>(best, MiniMax(board, depth + 1, i, !isMax));
					board.at(i) = EPiece::None;
				}
			}
			return best;
		}

		int best = 1000;

		for (int i = 0; i < boardWidth * boardWidth; i++)
		{
			if (board.at(i) == EPiece::None)
			{
				board.at(i) = playerPiece;
				best = std::min<int>(best, MiniMax(board, depth + 1, i, !isMax));
				board.at(i) = EPiece::None;
			}
		}
		return best;
	}

	int FindBestMove(Board board, EPiece piece)
	{
		int bestVal = -1000;
		int bestMove = -1;

		for (int i = 0; i < boardWidth * boardWidth; i++)
		{
			if (board.at(i) == EPiece::None)
			{
				board.at(i) = piece;
				const int moveVal = MiniMax(board, 0, i, false);
				board.at(i) = EPiece::None;

				if (moveVal > bestVal)
				{
					bestMove = i;
					bestVal = moveVal;
				}
			}
		}
		return bestMove;
	}
}

int main()
{
	game::App app;
	if (app.Construct(game::tileSize * game::boardWidth, game::tileSize * game::boardWidth, game::pixelSize, game::pixelSize))
		app.Start();

	return 0;
};
