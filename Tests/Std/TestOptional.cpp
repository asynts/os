#include <Tests/TestSuite.hpp>

#include <Std/Optional.hpp>

TEST_CASE(optional)
{
    Std::Optional<int> opt;

    ASSERT(opt.is_valid() == false);
    ASSERT(opt.value_or(42) == 42);

    opt = 13;

    ASSERT(opt.is_valid() == true);
    ASSERT(opt.value_or(42) == 13);
    ASSERT(opt.value() == 13);
    ASSERT(opt.must() == 13);
}

TEST_CASE(optional_destructors)
{
    Tests::Tracker::clear();

    {
        Std::Optional<Tests::Tracker> opt;

        Tests::Tracker::assert(0, 0, 0, 0);

        opt = Tests::Tracker { 42 };

        Tests::Tracker::assert(1, 1, 0, 1);
    }

    Tests::Tracker::assert(1, 1, 0, 2);
}

TEST_CASE(optional_moving)
{
    Tests::Tracker::clear();

    {
        Std::Optional<Tests::Tracker> opt1;

        Tests::Tracker::assert(0, 0, 0, 0);

        {
            Std::Optional<Tests::Tracker> opt2 { 42 };

            Tests::Tracker::assert(1, 1, 0, 1);

            {
                Std::Optional<Tests::Tracker> opt3 { 13 };

                Tests::Tracker::assert(2, 2, 0, 2);

                opt1 = opt3;

                Tests::Tracker::assert(2, 2, 1, 2);

                ASSERT(opt1.must() == 13);

                Tests::Tracker::assert(3, 2, 1, 3);
            }

            Tests::Tracker::assert(3, 2, 1, 4);

            opt2 = 7;

            Tests::Tracker::assert(4, 3, 1, 6);
        }

        Tests::Tracker::assert(4, 3, 1, 7);

        ASSERT(opt1.must() == 13);

        Tests::Tracker::assert(5, 3, 1, 8);

        opt1.clear();

        Tests::Tracker::assert(5, 3, 1, 9);
    }

    Tests::Tracker::assert(5, 3, 1, 9);
}

TEST_CASE(optional_move_clears)
{
    Std::Optional<int> opt1 = 42;

    Std::Optional<int> opt2 = move(opt1);

    ASSERT(!opt1.is_valid());
}

TEST_CASE(optional_rvalue_must_clears)
{
    Std::Optional<int> opt1 = 42;

    Std::Optional<int> opt2 = move(opt1).must();

    ASSERT(!opt1.is_valid());
}

TEST_MAIN();
